// Lossless WebP (VP8L) encoding.  See WebPEncode.h for why this exists.
//
// The bitstream follows RFC 9649 section 3, and comments below cite its
// subsections.  Implemented: the subtract-green and predictor transforms plus
// the entropy layer (length-limited prefix codes and LZ77 backward references).
// Deliberately not implemented, because each is an optional refinement:
// the colour transform, colour cache, colour indexing, meta prefix codes, and
// the transform-data / entropy-image subsampling beyond what the predictor
// itself needs.

#include "WebPEncode.h"

#include <stdint.h>
#include <string.h>
#include <algorithm>

namespace {

// ---------------------------------------------------------------------------
// Bit output (RFC 9649 3.7)
// ---------------------------------------------------------------------------

// Values are packed least significant bit first.  Prefix codes are not: they
// travel most significant bit first, which is why PutCode exists separately.
struct BitWriter {
    std::vector<BYTE> bytes;
    uint64_t acc = 0;
    int bits = 0;

    void Put(uint32_t value, int count) {
        if (count <= 0) return;
        if (count > 24) count = 24;
        const uint32_t mask = (1u << count) - 1u;
        acc |= (uint64_t)(value & mask) << bits;
        bits += count;
        while (bits >= 8) {
            bytes.push_back((BYTE)(acc & 0xFF));
            acc >>= 8;
            bits -= 8;
        }
    }

    void PutCode(uint32_t code, int count) {
        for (int i = count - 1; i >= 0; --i) Put((code >> i) & 1u, 1);
    }

    void Flush() {
        if (bits > 0) {
            bytes.push_back((BYTE)(acc & 0xFF));
            acc = 0;
            bits = 0;
        }
    }
};

// ---------------------------------------------------------------------------
// Pixels
// ---------------------------------------------------------------------------

// Internally every pixel is A in the top byte, then R, G, B - the order the
// format's channel names use, and the reverse of the byte order on disk.
inline int ChannelA(uint32_t v) { return (int)(v >> 24) & 0xFF; }
inline int ChannelR(uint32_t v) { return (int)(v >> 16) & 0xFF; }
inline int ChannelG(uint32_t v) { return (int)(v >> 8) & 0xFF; }
inline int ChannelB(uint32_t v) { return (int)v & 0xFF; }

inline uint32_t MakePixel(int a, int r, int g, int b) {
    return ((uint32_t)(a & 0xFF) << 24) | ((uint32_t)(r & 0xFF) << 16) |
           ((uint32_t)(g & 0xFF) << 8) | (uint32_t)(b & 0xFF);
}

inline uint32_t LoadPixel(const BYTE* p) {
    return ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[1] << 8) | (uint32_t)p[0];
}

// A predictor transform stores residual = actual - predicted, per channel and
// wrapping, and the decoder adds the prediction back (RFC 9649 3.5.1).
inline uint32_t SubPixel(uint32_t v, uint32_t pred) {
    return MakePixel(ChannelA(v) - ChannelA(pred), ChannelR(v) - ChannelR(pred),
                     ChannelG(v) - ChannelG(pred), ChannelB(v) - ChannelB(pred));
}

inline uint32_t Average2(uint32_t x, uint32_t y) {
    return MakePixel((ChannelA(x) + ChannelA(y)) / 2, (ChannelR(x) + ChannelR(y)) / 2,
                     (ChannelG(x) + ChannelG(y)) / 2, (ChannelB(x) + ChannelB(y)) / 2);
}

inline int ClampByte(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

inline uint32_t ClampAddSubtractFull(uint32_t l, uint32_t t, uint32_t tl) {
    return MakePixel(ClampByte(ChannelA(l) + ChannelA(t) - ChannelA(tl)),
                     ClampByte(ChannelR(l) + ChannelR(t) - ChannelR(tl)),
                     ClampByte(ChannelG(l) + ChannelG(t) - ChannelG(tl)),
                     ClampByte(ChannelB(l) + ChannelB(t) - ChannelB(tl)));
}

// Select predictor: pick whichever of left and top is closer to the gradient
// estimate, comparing whole pixels by Manhattan distance (RFC 9649 3.5.1).
inline uint32_t SelectPixel(uint32_t l, uint32_t t, uint32_t tl) {
    const int estA = ChannelA(l) + ChannelA(t) - ChannelA(tl);
    const int estR = ChannelR(l) + ChannelR(t) - ChannelR(tl);
    const int estG = ChannelG(l) + ChannelG(t) - ChannelG(tl);
    const int estB = ChannelB(l) + ChannelB(t) - ChannelB(tl);
    const int distL = ((estA - ChannelA(l)) < 0 ? ChannelA(l) - estA : estA - ChannelA(l)) +
                      ((estR - ChannelR(l)) < 0 ? ChannelR(l) - estR : estR - ChannelR(l)) +
                      ((estG - ChannelG(l)) < 0 ? ChannelG(l) - estG : estG - ChannelG(l)) +
                      ((estB - ChannelB(l)) < 0 ? ChannelB(l) - estB : estB - ChannelB(l));
    const int distT = ((estA - ChannelA(t)) < 0 ? ChannelA(t) - estA : estA - ChannelA(t)) +
                      ((estR - ChannelR(t)) < 0 ? ChannelR(t) - estR : estR - ChannelR(t)) +
                      ((estG - ChannelG(t)) < 0 ? ChannelG(t) - estG : estG - ChannelG(t)) +
                      ((estB - ChannelB(t)) < 0 ? ChannelB(t) - estB : estB - ChannelB(t));
    return (distL < distT) ? l : t;
}

const int kPredictionModes = 14;

// Mode 13 is the one predictor that needs signed division - clamp(a + (a-b)/2)
// with a = Average2(L, T) and b = TL - and C truncates toward zero, so it cannot
// share the per-channel clamp helper above.
inline uint32_t ClampAddSubtractHalf(uint32_t a, uint32_t b) {
    const int chans[4] = { ChannelA(a), ChannelR(a), ChannelG(a), ChannelB(a) };
    const int base[4]  = { ChannelA(b), ChannelR(b), ChannelG(b), ChannelB(b) };
    int out[4];
    for (int i = 0; i < 4; ++i) out[i] = ClampByte(chans[i] + (chans[i] - base[i]) / 2);
    return MakePixel(out[0], out[1], out[2], out[3]);
}

uint32_t PredictWithMode(int mode, uint32_t l, uint32_t t, uint32_t tl, uint32_t tr) {
    switch (mode) {
        case 0:  return 0xFF000000u;                       // solid black
        case 1:  return l;
        case 2:  return t;
        case 3:  return tr;
        case 4:  return tl;
        case 5:  return Average2(Average2(l, tr), t);
        case 6:  return Average2(l, tl);
        case 7:  return Average2(l, t);
        case 8:  return Average2(tl, t);
        case 9:  return Average2(t, tr);
        case 10: return Average2(Average2(l, tl), Average2(t, tr));
        case 11: return SelectPixel(l, t, tl);
        case 12: return ClampAddSubtractFull(l, t, tl);
        case 13: return ClampAddSubtractHalf(Average2(l, t), tl);
        default: return t;
    }
}

// ---------------------------------------------------------------------------
// Prefix codes (RFC 9649 3.7.2.1)
// ---------------------------------------------------------------------------

const int kMaxCodeLength = 15;   // a channel code length field is 4 bits
const int kMaxCLCodeLength = 7;  // the code-length code's own lengths are 3 bits

struct Huffman {
    int alphabet = 0;
    bool single = false;   // one symbol: described with length 1, but costs no bits
    std::vector<uint8_t> len;
    std::vector<uint16_t> code;

    void Build(const std::vector<uint32_t>& counts, int maxLen);
    void WriteDescription(BitWriter& bw) const;
    void WriteSymbol(BitWriter& bw, int symbol) const {
        // A one-symbol tree costs nothing per pixel (RFC 9649 3.7.2.1): the
        // decoder knows the value, so there are no bits to read.
        if (single) return;
        bw.PutCode(code[symbol], len[symbol]);
    }
    int SymbolBits(int symbol) const { return single ? 0 : len[symbol]; }
};

struct TreeNode {
    uint64_t weight;
    int child0;   // negative for a leaf, in which case child1 is the symbol
    int child1;
};

// Huffman depths via a two-queue merge: the leaves are sorted once, and because
// merged nodes come out in non-decreasing weight order too, no heap is needed.
void TreeDepths(const std::vector<uint64_t>& weight, const std::vector<int>& used,
                std::vector<int>& depthOut, int alphabet) {
    const int leafCount = (int)used.size();
    std::vector<int> leaves = used;
    std::sort(leaves.begin(), leaves.end(), [&](int a, int b) {
        if (weight[a] != weight[b]) return weight[a] < weight[b];
        return a < b;
    });

    std::vector<TreeNode> nodes;
    nodes.reserve((size_t)(2 * leafCount - 1));
    for (int s : leaves) nodes.push_back({ weight[s], -1, s });

    int leafHead = 0;
    int nodeHead = leafCount;
    for (int step = 0; step < leafCount - 1; ++step) {
        int pick[2];
        for (int k = 0; k < 2; ++k) {
            const bool hasLeaf = (leafHead < leafCount);
            const bool hasNode = (nodeHead < (int)nodes.size());
            if (hasLeaf && (!hasNode || nodes[leafHead].weight <= nodes[nodeHead].weight)) {
                pick[k] = leafHead++;
            } else {
                pick[k] = nodeHead++;
            }
        }
        nodes.push_back({ nodes[pick[0]].weight + nodes[pick[1]].weight, pick[0], pick[1] });
    }

    depthOut.assign(alphabet, 0);
    std::vector<int> stackNode, stackDepth;
    stackNode.push_back((int)nodes.size() - 1);
    stackDepth.push_back(0);
    while (!stackNode.empty()) {
        const int node = stackNode.back();
        const int depth = stackDepth.back();
        stackNode.pop_back();
        stackDepth.pop_back();
        if (nodes[node].child0 < 0) {
            depthOut[nodes[node].child1] = depth;
        } else {
            stackNode.push_back(nodes[node].child0);
            stackDepth.push_back(depth + 1);
            stackNode.push_back(nodes[node].child1);
            stackDepth.push_back(depth + 1);
        }
    }
}

void Huffman::Build(const std::vector<uint32_t>& counts, int maxLen) {
    alphabet = (int)counts.size();
    single = false;
    len.assign(alphabet, 0);
    code.assign(alphabet, 0);

    std::vector<int> used;
    for (int i = 0; i < alphabet; ++i) {
        if (counts[i] != 0) used.push_back(i);
    }
    if (used.empty()) {
        // Nothing is emitted for this alphabet.  The format has no way to say
        // "no code at all", so a single symbol stands in for it (3.7.2.1).
        len[0] = 1;
        single = true;
        return;
    }
    if (used.size() == 1) {
        // "the single leaf node value is marked with the length of 1 - even when
        // no bits are consumed when that single leaf node tree is used"
        len[used[0]] = 1;
        single = true;
        return;
    }

    std::vector<uint64_t> weight(counts.begin(), counts.end());
    std::vector<int> depth(alphabet, 0);
    uint64_t floorFreq = 1;
    for (int attempt = 0; attempt < 40; ++attempt) {
        TreeDepths(weight, used, depth, alphabet);
        int deepest = 0;
        for (int s : used) deepest = std::max(deepest, depth[s]);
        if (deepest <= maxLen) break;
        // The tree is deeper than the format can describe.  Raising the rarest
        // symbols' weights flattens it, and the ladder ends at "every weight
        // equal", whose tree is only ceil(log2(n)) deep - so this terminates.
        floorFreq *= 2;
        for (int s : used) weight[s] = std::max(weight[s], floorFreq);
    }
    for (int s : used) len[s] = (uint8_t)depth[s];

    // Canonical codes: sorted by length, then by symbol within a length.
    int deepest = 0;
    for (int s : used) deepest = std::max(deepest, (int)len[s]);
    uint32_t next = 0;
    for (int l = 1; l <= deepest; ++l) {
        for (int s = 0; s < alphabet; ++s) {
            if (len[s] == l) code[s] = (uint16_t)next++;
        }
        next <<= 1;
    }
}

void Huffman::WriteDescription(BitWriter& bw) const {
    int firstSymbol = -1, secondSymbol = -1, usedCount = 0;
    for (int s = 0; s < alphabet; ++s) {
        if (len[s] != 0) {
            ++usedCount;
            if (firstSymbol < 0) firstSymbol = s;
            else if (secondSymbol < 0) secondSymbol = s;
        }
    }

    if (usedCount <= 2) {
        // "Simple Code Length Code": one or two symbols of length 1, everything
        // else implicitly zero.  Worth taking: it is a handful of bits.
        bw.Put(1, 1);
        bw.Put(usedCount <= 1 ? 0u : 1u, 1);
        const int firstIs8 = (firstSymbol > 1) ? 1 : 0;
        bw.Put((uint32_t)firstIs8, 1);
        bw.Put((uint32_t)firstSymbol, firstIs8 ? 8 : 1);
        if (usedCount >= 2) bw.Put((uint32_t)secondSymbol, 8);
        return;
    }

    bw.Put(0, 1);   // "Normal Code Length Code"

    // The code lengths themselves, as a symbol stream over 0..15 plus the repeat
    // codes 17 (3..10 zeros) and 18 (11..138 zeros).  Long zero runs are the
    // common case - an alphabet is mostly unused symbols.
    std::vector<int> symbols;
    std::vector<int> extraValue;
    std::vector<int> extraBitCount;
    for (int i = 0; i < alphabet; ) {
        if (len[i] != 0) {
            symbols.push_back(len[i]);
            extraValue.push_back(0);
            extraBitCount.push_back(0);
            ++i;
            continue;
        }
        int run = 0;
        while (i + run < alphabet && len[i + run] == 0) ++run;
        while (run >= 11) {
            const int take = std::min(run, 138);
            symbols.push_back(18);
            extraValue.push_back(take - 11);
            extraBitCount.push_back(7);
            run -= take;
            i += take;
        }
        if (run >= 3) {
            symbols.push_back(17);
            extraValue.push_back(run - 3);
            extraBitCount.push_back(3);
            i += run;
            run = 0;
        }
        while (run > 0) {
            symbols.push_back(0);
            extraValue.push_back(0);
            extraBitCount.push_back(0);
            ++i;
            --run;
        }
    }

    std::vector<uint32_t> clCounts(19, 0);
    for (int s : symbols) clCounts[s]++;

    Huffman cl;
    cl.Build(clCounts, kMaxCLCodeLength);

    static const int kOrder[19] = { 17, 18, 0, 1, 2, 3, 4, 5, 16, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
    int numCodeLengths = 4;
    for (int i = 0; i < 19; ++i) {
        if (cl.len[kOrder[i]] != 0) numCodeLengths = i + 1;
    }
    bw.Put((uint32_t)(numCodeLengths - 4), 4);
    for (int i = 0; i < numCodeLengths; ++i) {
        bw.Put((uint32_t)cl.len[kOrder[i]], 3);
    }

    // A zero here means "the whole alphabet follows", which is what we write:
    // the max_symbol shortcut would only save a few bits.
    bw.Put(0, 1);

    for (size_t k = 0; k < symbols.size(); ++k) {
        cl.WriteSymbol(bw, symbols[k]);
        if (extraBitCount[k] > 0) bw.Put((uint32_t)extraValue[k], extraBitCount[k]);
    }
}

// ---------------------------------------------------------------------------
// LZ77 prefix coding (RFC 9649 3.6.2.2)
// ---------------------------------------------------------------------------

// Lengths and distances share one prefix scheme: prefix code p < 4 means the
// value p + 1 outright; above that, code p covers "offset + 2^extra" values.
void PrefixForValue(int value, int maxCode, int& code, int& extraBits, int& offset) {
    if (value <= 4) {
        code = value - 1;
        extraBits = 0;
        offset = value - 1;
        return;
    }
    for (int p = 4; p <= maxCode; ++p) {
        const int extra = (p - 2) >> 1;
        const int base = (2 + (p & 1)) << extra;
        if (value <= base + (1 << extra)) {
            code = p;
            extraBits = extra;
            offset = base;
            return;
        }
    }
    code = maxCode;
    extraBits = (maxCode - 2) >> 1;
    offset = (2 + (maxCode & 1)) << extraBits;
}

// A match copies whole pixels, so a length is capped by the 24 length codes
// (4096) and a distance by the distance alphabet's largest code (1048576).
// Distances travel as "scan-line distance + 120", which skips the 120-entry
// neighbourhood table of 3.6.2.2.1 entirely.
const int kMaxMatchLength = 4095;
const int kMaxBackDistance = 1048576 - 120;
const int kMinMatch = 4;

void LengthPrefix(int length, int& code, int& extraBits, int& offset) {
    PrefixForValue(length, 23, code, extraBits, offset);
}

void DistancePrefix(int distance, int& code, int& extraBits, int& offset) {
    PrefixForValue(distance + 120, 39, code, extraBits, offset);
}

// One pixel's worth of output: either a literal pixel or a backward reference.
// A literal is packed == 0, which no match can be, since a match always has a
// length of at least 1 in the top bits.
struct Token {
    uint32_t pixel;
    uint32_t packed;   // 0 for a literal, else (length << 20) | distance
};

Token MakeLiteral(uint32_t pixel) { return Token{ pixel, 0 }; }

Token MakeMatch(int length, int distance) {
    return Token{ 0, ((uint32_t)length << 20) | (uint32_t)distance };
}

int TokenLength(const Token& t) { return (int)(t.packed >> 20); }
int TokenDistance(const Token& t) { return (int)(t.packed & 0xFFFFFu); }

// ---------------------------------------------------------------------------
// Choosing between literals and backward references
// ---------------------------------------------------------------------------

struct ImageCodes {
    Huffman green;   // 256 literals + 24 length codes
    Huffman red;
    Huffman blue;
    Huffman alpha;
    Huffman dist;    // 40
};

// The codes decide whether a match is worth taking, so the chooser takes a cost
// model: the first pass has no codes yet and assumes a literal pixel costs about
// three bytes, and the second pass uses the lengths the first pass produced.
struct CostModel {
    const ImageCodes* codes = nullptr;

    int LiteralBits(uint32_t px) const {
        if (!codes) return 24;
        return codes->green.SymbolBits(ChannelG(px)) + codes->red.SymbolBits(ChannelR(px)) +
               codes->blue.SymbolBits(ChannelB(px)) + codes->alpha.SymbolBits(ChannelA(px));
    }

    int MatchBits(int length, int distance) const {
        int lp, lb, loff;
        LengthPrefix(length, lp, lb, loff);
        int dp, db, doff;
        DistancePrefix(distance, dp, db, doff);
        const int green = codes ? codes->green.SymbolBits(256 + lp) : 8;
        const int dist = codes ? codes->dist.SymbolBits(dp) : 8;
        return green + lb + dist + db;
    }
};

inline uint32_t Hash3(uint32_t a, uint32_t b, uint32_t c) {
    uint32_t h = a * 0x9E3779B1u;
    h ^= (b * 0x85EBCA77u) >> 11;
    h ^= (c * 0xC2B2AE3Du) >> 17;
    h ^= h >> 15;
    return h;
}

const int kHashBits = 16;
const int kMaxChain = 64;

void ChooseTokens(const std::vector<uint32_t>& px, const CostModel& model,
                  std::vector<Token>& out) {
    const size_t n = px.size();
    out.clear();
    out.reserve(n);

    std::vector<int32_t> head((size_t)1 << kHashBits, -1);
    std::vector<int32_t> prev(n, -1);
    const uint32_t hashShift = 32 - kHashBits;

    size_t p = 0;
    while (p < n) {
        const bool hashable = (p + 2 < n);
        uint32_t slot = 0;
        if (hashable) {
            slot = Hash3(px[p], px[p + 1], px[p + 2]) >> hashShift;
            prev[p] = head[slot];
            head[slot] = (int32_t)p;
        }

        int bestLength = 0, bestDistance = 0, bestScore = 0;
        if (hashable) {
            const int maxLength = (int)std::min<size_t>((size_t)kMaxMatchLength, n - p);
            int32_t candidate = prev[p];
            for (int step = 0; candidate >= 0 && step < kMaxChain; ++step, candidate = prev[candidate]) {
                const size_t q = (size_t)candidate;
                const int distance = (int)(p - q);
                if (distance > kMaxBackDistance) break;   // chain is newest-first, so are the rest

                int length = 0, literalBits = 0;
                while (length < maxLength && px[q + length] == px[p + length]) {
                    literalBits += model.LiteralBits(px[p + length]);
                    ++length;
                }
                if (length < kMinMatch) continue;

                const int score = literalBits - model.MatchBits(length, distance);
                if (score > bestScore || (score == bestScore && length > bestLength)) {
                    bestScore = score;
                    bestLength = length;
                    bestDistance = distance;
                }
            }
        }

        if (bestLength >= kMinMatch && bestScore > 0) {
            out.push_back(MakeMatch(bestLength, bestDistance));
            // The copied pixels are themselves worth indexing: later matches may
            // point into them.
            for (int k = 1; k < bestLength; ++k) {
                const size_t q = p + k;
                if (q + 2 < n) {
                    const uint32_t inner = Hash3(px[q], px[q + 1], px[q + 2]) >> hashShift;
                    prev[q] = head[inner];
                    head[inner] = (int32_t)q;
                }
            }
            p += bestLength;
        } else {
            out.push_back(MakeLiteral(px[p]));
            ++p;
        }
    }
}

void BuildImageCodes(const std::vector<Token>& tokens, ImageCodes& codes) {
    std::vector<uint32_t> greenCounts(256 + 24, 0);
    std::vector<uint32_t> redCounts(256, 0);
    std::vector<uint32_t> blueCounts(256, 0);
    std::vector<uint32_t> alphaCounts(256, 0);
    std::vector<uint32_t> distCounts(40, 0);

    for (const Token& t : tokens) {
        if (t.packed == 0) {
            greenCounts[ChannelG(t.pixel)]++;
            redCounts[ChannelR(t.pixel)]++;
            blueCounts[ChannelB(t.pixel)]++;
            alphaCounts[ChannelA(t.pixel)]++;
            continue;
        }
        int lp, lb, loff;
        LengthPrefix(TokenLength(t), lp, lb, loff);
        greenCounts[256 + lp]++;
        int dp, db, doff;
        DistancePrefix(TokenDistance(t), dp, db, doff);
        distCounts[dp]++;
    }

    codes.green.Build(greenCounts, kMaxCodeLength);
    codes.red.Build(redCounts, kMaxCodeLength);
    codes.blue.Build(blueCounts, kMaxCodeLength);
    codes.alpha.Build(alphaCounts, kMaxCodeLength);
    codes.dist.Build(distCounts, kMaxCodeLength);
}

void WriteCodes(BitWriter& bw, const ImageCodes& codes) {
    codes.green.WriteDescription(bw);
    codes.red.WriteDescription(bw);
    codes.blue.WriteDescription(bw);
    codes.alpha.WriteDescription(bw);
    codes.dist.WriteDescription(bw);
}

// RFC 9649 3.7.2.3: a literal is green, then red, blue and alpha; a backward
// reference is a length code in the green alphabet plus a distance code.
void WriteTokens(BitWriter& bw, const std::vector<Token>& tokens, const ImageCodes& codes) {
    for (const Token& t : tokens) {
        if (t.packed == 0) {
            codes.green.WriteSymbol(bw, ChannelG(t.pixel));
            codes.red.WriteSymbol(bw, ChannelR(t.pixel));
            codes.blue.WriteSymbol(bw, ChannelB(t.pixel));
            codes.alpha.WriteSymbol(bw, ChannelA(t.pixel));
            continue;
        }
        const int length = TokenLength(t);
        int lp, lb, loff;
        LengthPrefix(length, lp, lb, loff);
        codes.green.WriteSymbol(bw, 256 + lp);
        bw.Put((uint32_t)(length - loff - 1), lb);

        const int distance = TokenDistance(t);
        int dp, db, doff;
        DistancePrefix(distance, dp, db, doff);
        codes.dist.WriteSymbol(bw, dp);
        bw.Put((uint32_t)(distance + 120 - doff - 1), db);
    }
}

// ---------------------------------------------------------------------------
// The predictor transform (RFC 9649 3.5.1)
// ---------------------------------------------------------------------------

// The prediction for a pixel, with the border rules of 3.5.1: the very first
// pixel predicts solid black, the top row always predicts from the left, the
// left column always from above, and on the rightmost column the top-right pixel
// is taken from the start of the same row.
uint32_t PredictAt(const std::vector<uint32_t>& src, int w, int /*h*/, int x, int y, int mode) {
    if (x == 0 && y == 0) return 0xFF000000u;
    if (y == 0) return src[(size_t)x - 1];
    if (x == 0) return src[(size_t)(y - 1) * w];
    const uint32_t l = src[(size_t)y * w + x - 1];
    const uint32_t t = src[(size_t)(y - 1) * w + x];
    const uint32_t tl = src[(size_t)(y - 1) * w + x - 1];
    const uint32_t tr = (x == w - 1) ? src[(size_t)y * w] : src[(size_t)(y - 1) * w + x + 1];
    return PredictWithMode(mode, l, t, tl, tr);
}

// How expensive a residual looks, used only to pick between modes: distance from
// zero, so a predictor that lands near the real value wins.
int ResidualMagnitude(uint32_t value, uint32_t predicted) {
    const uint32_t residual = SubPixel(value, predicted);
    int total = 0;
    const int channels[4] = { ChannelA(residual), ChannelR(residual), ChannelG(residual), ChannelB(residual) };
    for (int i = 0; i < 4; ++i) {
        total += (channels[i] < 128) ? channels[i] : 256 - channels[i];
    }
    return total;
}

// The mode image is a sub-resolution image whose green channel carries one
// predictor mode per tile.  It is an "entropy-coded-image" (RFC 9649 3.8.3):
// colour cache info, then five prefix codes, then pixels - no transforms, and no
// meta prefix code bit, which only the top-level image has.
void WriteModeImage(BitWriter& bw, const uint32_t* px, int w, int h) {
    std::vector<uint32_t> greenCounts(256 + 24, 0), redCounts(256, 0);
    std::vector<uint32_t> blueCounts(256, 0), alphaCounts(256, 0), distCounts(40, 0);
    for (int i = 0; i < w * h; ++i) {
        greenCounts[ChannelG(px[i])]++;
        redCounts[ChannelR(px[i])]++;
        blueCounts[ChannelB(px[i])]++;
        alphaCounts[ChannelA(px[i])]++;
    }

    ImageCodes codes;
    codes.green.Build(greenCounts, kMaxCodeLength);
    codes.red.Build(redCounts, kMaxCodeLength);
    codes.blue.Build(blueCounts, kMaxCodeLength);
    codes.alpha.Build(alphaCounts, kMaxCodeLength);
    codes.dist.Build(distCounts, kMaxCodeLength);   // empty: stands in as symbol 0

    bw.Put(0, 1);   // no colour cache
    WriteCodes(bw, codes);
    for (int i = 0; i < w * h; ++i) {
        codes.green.WriteSymbol(bw, ChannelG(px[i]));
        codes.red.WriteSymbol(bw, ChannelR(px[i]));
        codes.blue.WriteSymbol(bw, ChannelB(px[i]));
        codes.alpha.WriteSymbol(bw, ChannelA(px[i]));
    }
}

// 128x128 tiles: fine enough that a mode suited to one part of the screen is not
// forced on another, coarse enough that the mode image stays a few dozen bytes.
const int kTileBits = 7;

void EncodeVP8L(const std::vector<uint32_t>& pixels, int w, int h, bool hasAlpha,
                BitWriter& bw) {
    // Forward transforms, applied in the order the decoder will invert them
    // last-first.  Subtract green first: it levels red and blue against green, so
    // the predictor below has less to do.
    std::vector<uint32_t> transformed = pixels;
    for (size_t i = 0; i < transformed.size(); ++i) {
        const int g = ChannelG(transformed[i]);
        transformed[i] = MakePixel(ChannelA(transformed[i]), ChannelR(transformed[i]) - g, g,
                                   ChannelB(transformed[i]) - g);
    }

    const int tw = (w + (1 << kTileBits) - 1) >> kTileBits;
    const int th = (h + (1 << kTileBits) - 1) >> kTileBits;
    std::vector<uint32_t> modeImage((size_t)tw * th);

    for (int ty = 0; ty < th; ++ty) {
        const int yEnd = std::min((ty + 1) << kTileBits, h);
        for (int tx = 0; tx < tw; ++tx) {
            const int xEnd = std::min((tx + 1) << kTileBits, w);
            int bestMode = 2;
            int bestCost = 0x7FFFFFFF;
            for (int mode = 0; mode < kPredictionModes; ++mode) {
                int cost = 0;
                // Every second pixel in each direction: enough to rank the modes,
                // a quarter of the work.
                for (int y = ty << kTileBits; y < yEnd; y += 2) {
                    for (int x = tx << kTileBits; x < xEnd; x += 2) {
                        const size_t at = (size_t)y * w + x;
                        cost += ResidualMagnitude(transformed[at],
                                                  PredictAt(transformed, w, h, x, y, mode));
                    }
                }
                if (cost < bestCost) {
                    bestCost = cost;
                    bestMode = mode;
                }
            }
            modeImage[(size_t)ty * tw + tx] = MakePixel(0, 0, bestMode, 0);
        }
    }

    std::vector<uint32_t> residuals((size_t)w * h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int tile = ChannelG(modeImage[(size_t)(y >> kTileBits) * tw + (x >> kTileBits)]);
            const size_t at = (size_t)y * w + x;
            residuals[at] = SubPixel(transformed[at], PredictAt(transformed, w, h, x, y, tile));
        }
    }

    // Choose references once with no cost information, then again with the code
    // lengths that produced.  The codes are rebuilt from the second choice, so
    // every symbol that gets written is guaranteed a code.
    std::vector<Token> tokens;
    CostModel rough;
    ChooseTokens(residuals, rough, tokens);

    ImageCodes codes;
    BuildImageCodes(tokens, codes);

    CostModel refined;
    refined.codes = &codes;
    ChooseTokens(residuals, refined, tokens);
    BuildImageCodes(tokens, codes);

    bw.bytes.reserve(residuals.size() + residuals.size() / 2 + 4096);
    bw.Put(0x2F, 8);                     // VP8L signature
    bw.Put((uint32_t)(w - 1), 14);
    bw.Put((uint32_t)(h - 1), 14);
    bw.Put(hasAlpha ? 1u : 0u, 1);
    bw.Put(0, 3);                        // version

    bw.Put(1, 1);
    bw.Put(2, 2);                        // subtract green, which carries no data
    bw.Put(1, 1);
    bw.Put(0, 2);                        // predictor
    bw.Put((uint32_t)(kTileBits - 2), 3);
    WriteModeImage(bw, modeImage.data(), tw, th);
    bw.Put(0, 1);                        // no further transforms

    bw.Put(0, 1);                        // no colour cache
    bw.Put(0, 1);                        // a single meta prefix code
    WriteCodes(bw, codes);
    WriteTokens(bw, tokens, codes);
    bw.Flush();
}

void AppendLE32(std::vector<BYTE>& out, uint32_t value) {
    out.push_back((BYTE)(value & 0xFF));
    out.push_back((BYTE)((value >> 8) & 0xFF));
    out.push_back((BYTE)((value >> 16) & 0xFF));
    out.push_back((BYTE)((value >> 24) & 0xFF));
}

void AppendTag(std::vector<BYTE>& out, const char* tag) {
    for (int i = 0; i < 4; ++i) out.push_back((BYTE)tag[i]);
}

}  // namespace

bool EncodeBGRAtoWebP(const BYTE* pixels, int width, int height, int stride,
                      bool hasAlpha, std::vector<BYTE>& out) {
    out.clear();
    if (!pixels || width <= 0 || height <= 0 || width > 16384 || height > 16384) return false;

    std::vector<uint32_t> pixels32;
    pixels32.resize((size_t)width * height);
    for (int y = 0; y < height; ++y) {
        const BYTE* row = pixels + (ptrdiff_t)y * stride;
        for (int x = 0; x < width; ++x) {
            pixels32[(size_t)y * width + x] = LoadPixel(row + (size_t)x * 4);
        }
    }

    BitWriter bw;
    EncodeVP8L(pixels32, width, height, hasAlpha, bw);

    const uint32_t payload = (uint32_t)bw.bytes.size();
    const uint32_t padding = payload & 1u;
    const uint32_t riffSize = 4u + 8u + payload + padding;   // "WEBP" + chunk header + chunk

    out.reserve(20 + payload + padding);
    AppendTag(out, "RIFF");
    AppendLE32(out, riffSize);
    AppendTag(out, "WEBP");
    AppendTag(out, "VP8L");
    AppendLE32(out, payload);
    out.insert(out.end(), bw.bytes.begin(), bw.bytes.end());
    if (padding) out.push_back(0);
    return true;
}


