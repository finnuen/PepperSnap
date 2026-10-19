#include "PepperSnapInternal.h"

// ----------------------------------------------------------------------------
// Pixelation & Vector Annotation Renderers
// ----------------------------------------------------------------------------

static void ApplyPixelateFilterRawBuffer(DWORD* bufPixels, int bw, int bh, int strideBytes, int rx, int ry, int rw, int rh, int blockSize, bool elliptical) {
    if (!bufPixels || rw <= 2 || rh <= 2) return;
    int x0 = std::max(0, std::min(bw - 1, rx));
    int y0 = std::max(0, std::min(bh - 1, ry));
    int x1 = std::max(0, std::min(bw, rx + rw));
    int y1 = std::max(0, std::min(bh, ry + rh));
    if (x1 <= x0 || y1 <= y0) return;

    blockSize = std::max(2, blockSize);
    int subW = x1 - x0;
    int subH = y1 - y0;
    BYTE* scan0 = (BYTE*)bufPixels + (size_t)y0 * strideBytes + (size_t)x0 * 4;

    float cx = rw * 0.5f;
    float cy = rh * 0.5f;
    float invRadX = 1.0f / std::max(1.0f, rw * 0.5f);
    float invRadY = 1.0f / std::max(1.0f, rh * 0.5f);
    float baseLocalX = (float)(x0 - rx) + 0.5f;
    float baseLocalY = (float)(y0 - ry) + 0.5f;

    for (int by = 0; by < subH; by += blockSize) {
        for (int bx = 0; bx < subW; bx += blockSize) {
            long sumR = 0, sumG = 0, sumB = 0, count = 0;
            int maxY = std::min(subH, by + blockSize);
            int maxX = std::min(subW, bx + blockSize);

            for (int py = by; py < maxY; ++py) {
                DWORD* row = (DWORD*)(scan0 + (size_t)py * strideBytes);
                for (int px = bx; px < maxX; ++px) {
                    DWORD argb = row[px];
                    sumB += (argb & 0xFF);
                    sumG += ((argb >> 8) & 0xFF);
                    sumR += ((argb >> 16) & 0xFF);
                    count++;
                }
            }
            if (count > 0) {
                DWORD outPx = (0xFFu << 24) | ((DWORD)(sumR / count) << 16) | ((DWORD)(sumG / count) << 8) | (DWORD)(sumB / count);
                if (!elliptical) {
                    for (int py = by; py < maxY; ++py) {
                        DWORD* row = (DWORD*)(scan0 + (size_t)py * strideBytes);
                        for (int px = bx; px < maxX; ++px) {
                            row[px] = outPx;
                        }
                    }
                } else {
                    for (int py = by; py < maxY; ++py) {
                        DWORD* row = (DWORD*)(scan0 + (size_t)py * strideBytes);
                        float ny = ((baseLocalY + (float)py) - cy) * invRadY;
                        float nySq = ny * ny;
                        if (nySq > 1.0f) continue;
                        for (int px = bx; px < maxX; ++px) {
                            float nx = ((baseLocalX + (float)px) - cx) * invRadX;
                            if (nx * nx + nySq <= 1.0f) {
                                row[px] = outPx;
                            }
                        }
                    }
                }
            }
        }
    }
}

void PepperSnapDaemon::ApplyPixelateFilter(Bitmap* bmp, int rx, int ry, int rw, int rh, int blockSize, bool elliptical) {
    if (!bmp || rw <= 2 || rh <= 2) return;
    int bw = (int)bmp->GetWidth();
    int bh = (int)bmp->GetHeight();
    Rect lockRect(0, 0, bw, bh);
    BitmapData data;
    if (bmp->LockBits(&lockRect, ImageLockModeRead | ImageLockModeWrite, PixelFormat32bppARGB, &data) != Ok) return;
    ApplyPixelateFilterRawBuffer((DWORD*)data.Scan0, bw, bh, data.Stride, rx, ry, rw, rh, blockSize, elliptical);
    bmp->UnlockBits(&data);
}

// ----------------------------------------------------------------------------
// Text annotation face, size and glyph outline
// ----------------------------------------------------------------------------
//
// A text box has no face or size control of its own: the S / M / L / XL buttons
// set its stroke width, and the type size is derived from that. Drawing,
// measuring and hit-testing a text box have to agree on all of it, or the caret,
// the selection box and the exported pixels drift apart - which is what five
// hand-copied copies of the size formula invited. These five helpers are the
// single source of truth.

// Calibri Light ships with Windows (calibril.ttf). Asking GDI+ for a family that
// is not installed is not a graceful failure: it hands back a Font that draws
// nothing at all, so the text would silently disappear. Probe once, fall back.
static const wchar_t* AnnotationFontFace() {
    static const wchar_t* face = nullptr;
    if (!face) {
        FontFamily probe(L"Calibri Light");
        face = (probe.GetLastStatus() == Ok) ? L"Calibri Light" : L"Segoe UI";
    }
    return face;
}

// Calibri Light is installed without a bold face (calibril.ttf is the only
// upright weight), so asking for bold makes GDI+ synthesise one. That is the
// weight the earlier Text style window shipped, so it is kept; switch this to
// FontStyleRegular for the true thin face.
static const INT kAnnotationFontStyle = FontStyleBold;

static float AnnotationFontSize(float strokeWidth) {
    return std::max(10.0f, strokeWidth * 3.8f + 6.0f);
}

// The outline is picked from the fill rather than exposed as a setting, which is
// what made the earlier Text style window inconvenient. Black is the shipped
// look for every swatch except black itself, which needs the light border to
// stay legible over a dark screenshot.
static Color AnnotationOutlineColor(const Color& fill) {
    // Lightness, the L of HSL - the value Color::GetBrightness() reports, spelled
    // out because the mingw GDI+ headers this builds against do not declare that
    // accessor.
    BYTE hi = std::max(std::max(fill.GetR(), fill.GetG()), fill.GetB());
    BYTE lo = std::min(std::min(fill.GetR(), fill.GetG()), fill.GetB());
    float lightness = (float)(hi + lo) / 510.0f;
    return (lightness >= 0.35f) ? Color(255, 0, 0, 0) : Color(255, 255, 255, 255);
}

// At least 2 px, at most 5% of the type size. The floor wins where the two
// disagree (below 40 px type), because a border thinner than that on small
// glyphs reads as anti-aliasing rather than as an outline. At the four shipped
// type sizes - 13.6 / 21.2 / 36.4 / 59.2 px - this gives 2 / 2 / 2 / 3 px.
static float AnnotationOutlineWidth(float fontSize) {
    return std::max(2.0f, fontSize * 0.05f);
}

void PepperSnapDaemon::DrawVectorAnnotation(Graphics& g, const Annotation& ann, int offsetX, int offsetY, bool isSelected, bool isEditingCaret, bool forceOpaqueHighlighter) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    float sx = ann.startPt.x - offsetX;
    float sy = ann.startPt.y - offsetY;
    float ex = ann.endPt.x   - offsetX;
    float ey = ann.endPt.y   - offsetY;

    switch (ann.tool) {
        case OverlayTool::Pen:
        case OverlayTool::Highlighter: {
            if (ann.points.empty()) break;
            bool isHi = (ann.tool == OverlayTool::Highlighter);
            BYTE alpha = isHi ? (forceOpaqueHighlighter ? 255 : 95) : 255;
            Color c = isHi ? Color(alpha, ann.color.GetR(), ann.color.GetG(), ann.color.GetB()) : ann.color;
            Pen pen(c, isHi ? ann.strokeWidth * 4.5f : ann.strokeWidth);
            pen.SetStartCap(LineCapRound);
            pen.SetEndCap(LineCapRound);
            pen.SetLineJoin(LineJoinRound);

            static thread_local std::vector<PointF> tlsPts;
            static thread_local std::vector<PointF> tlsSmooth;
            static thread_local std::vector<bool> tlsSharp;
            tlsPts.clear();
            if (tlsPts.capacity() < ann.points.size() + 2) tlsPts.reserve(ann.points.size() + 2);
            for (const auto& p : ann.points) tlsPts.emplace_back(p.x - offsetX, p.y - offsetY);

            if (tlsPts.size() == 1) {
                tlsPts.emplace_back(tlsPts[0].X + 0.5f, tlsPts[0].Y);
            }

            GraphicsState gst = g.Save();
            g.SetSmoothingMode(SmoothingModeHighQuality);
            g.SetPixelOffsetMode(PixelOffsetModeHighQuality);

            if (tlsPts.size() == 2) {
                float dx = tlsPts[1].X - tlsPts[0].X;
                float dy = tlsPts[1].Y - tlsPts[0].Y;
                if (dx * dx + dy * dy < 0.25f) tlsPts[1].X += 0.5f;
                g.DrawLine(&pen, tlsPts[0], tlsPts[1]);
            } else if (!g_Daemon.penSmoothingEnabled || g_Daemon.penSmoothingStrength <= 0) {
                // Raw unsmoothed polyline when smoothing is disabled in Options
                g.DrawLines(&pen, tlsPts.data(), (INT)tlsPts.size());
            } else {
                float sNorm = std::max(0.05f, std::min(1.0f, g_Daemon.penSmoothingStrength / 100.0f));
                // Mark vertices belonging to long Shift-snapped straight segments (> 48px) so they stay sharp
                size_t n = tlsPts.size();
                tlsSharp.assign(n, false);
                tlsSharp[0] = true;
                tlsSharp[n - 1] = true;
                for (size_t i = 0; i + 1 < n; ++i) {
                    float dx = tlsPts[i + 1].X - tlsPts[i].X;
                    float dy = tlsPts[i + 1].Y - tlsPts[i].Y;
                    if (dx * dx + dy * dy > 2304.0f) {
                        tlsSharp[i] = true;
                        tlsSharp[i + 1] = true;
                    }
                }

                // Continuous strength-scaled Gaussian centerline smoothing (0.25..4.5 passes)
                float totalGauss = sNorm * 4.5f;
                int fullPasses = (int)std::floor(totalGauss);
                float fracPass = totalGauss - (float)fullPasses;
                tlsSmooth.resize(n);
                auto runGaussianPass = [&](float weight) {
                    float sideW = 0.25f * weight;
                    float centerW = 1.0f - 2.0f * sideW;
                    tlsSmooth[0] = tlsPts[0];
                    tlsSmooth[n - 1] = tlsPts[n - 1];
                    for (size_t i = 1; i + 1 < n; ++i) {
                        if (tlsSharp[i]) {
                            tlsSmooth[i] = tlsPts[i];
                        } else {
                            tlsSmooth[i].X = sideW * tlsPts[i - 1].X + centerW * tlsPts[i].X + sideW * tlsPts[i + 1].X;
                            tlsSmooth[i].Y = sideW * tlsPts[i - 1].Y + centerW * tlsPts[i].Y + sideW * tlsPts[i + 1].Y;
                        }
                    }
                    tlsPts.swap(tlsSmooth);
                };
                for (int pass = 0; pass < fullPasses; ++pass) {
                    runGaussianPass(1.0f);
                }
                if (fracPass > 0.01f) {
                    runGaussianPass(fracPass);
                }

                // Chaikin quadratic B-spline corner subdivision (1 pass for <= 35%, 2 passes for > 35%)
                int chaikinPasses = (g_Daemon.penSmoothingStrength <= 35) ? 1 : 2;
                for (int pass = 0; pass < chaikinPasses; ++pass) {
                    tlsSmooth.clear();
                    tlsSmooth.reserve(tlsPts.size() * 2);
                    tlsSmooth.push_back(tlsPts.front());
                    for (size_t i = 0; i + 1 < tlsPts.size(); ++i) {
                        const PointF& p0 = tlsPts[i];
                        const PointF& p1 = tlsPts[i + 1];
                        float dx = p1.X - p0.X;
                        float dy = p1.Y - p0.Y;
                        if (dx * dx + dy * dy > 2304.0f) {
                            if (i > 0) tlsSmooth.push_back(p0);
                            tlsSmooth.push_back(p1);
                        } else {
                            tlsSmooth.emplace_back(0.75f * p0.X + 0.25f * p1.X, 0.75f * p0.Y + 0.25f * p1.Y);
                            tlsSmooth.emplace_back(0.25f * p0.X + 0.75f * p1.X, 0.25f * p0.Y + 0.75f * p1.Y);
                        }
                    }
                    tlsSmooth.push_back(tlsPts.back());
                    tlsPts.swap(tlsSmooth);
                }

                g.DrawLines(&pen, tlsPts.data(), (INT)tlsPts.size());
            }
            g.Restore(gst);
            break;
        }

        case OverlayTool::Line: {
            // Pure shadow-free vector line
            Pen pen(ann.color, ann.strokeWidth);
            pen.SetStartCap(LineCapRound);
            pen.SetEndCap(LineCapRound);
            g.DrawLine(&pen, sx, sy, ex, ey);
            break;
        }

        case OverlayTool::Arrow: {
            // Shadow-free vector arrow geometry calculation
            float dx = ex - sx, dy = ey - sy;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len < 2.0f) break;
            float angle = std::atan2(dy, dx);
            float headLen = std::max(14.0f, ann.strokeWidth * 3.8f);
            float headAngle = 0.48f;
            float shaftLen = std::max(0.0f, len - headLen * 0.78f);

            if (shaftLen > 1.0f) {
                Pen pen(ann.color, ann.strokeWidth);
                pen.SetStartCap(LineCapRound);
                pen.SetEndCap(LineCapFlat);
                g.DrawLine(&pen, sx, sy, sx + shaftLen * std::cos(angle), sy + shaftLen * std::sin(angle));
            }

            PointF tri[3] = {
                PointF(ex, ey),
                PointF(ex - headLen * std::cos(angle - headAngle), ey - headLen * std::sin(angle - headAngle)),
                PointF(ex - headLen * std::cos(angle + headAngle), ey - headLen * std::sin(angle + headAngle))
            };
            SolidBrush headBrush(ann.color);
            g.FillPolygon(&headBrush, tri, 3);
            break;
        }

        case OverlayTool::NumberArrow: {
            // Numbered circle badge at startPt + shadow-free arrow pointing to endPt
            float r = std::max(14.0f, ann.strokeWidth * 2.0f + 8.0f);
            float dx = ex - sx, dy = ey - sy;
            float len = std::sqrt(dx * dx + dy * dy);

            if (len > r + 4.0f) {
                float angle = std::atan2(dy, dx);
                float headLen = std::max(14.0f, ann.strokeWidth * 3.8f);
                float headAngle = 0.48f;
                float lineStartX = sx + r * std::cos(angle);
                float lineStartY = sy + r * std::sin(angle);
                float shaftEndDist = std::max(r, len - headLen * 0.78f);

                if (shaftEndDist > r + 1.0f) {
                    Pen pen(ann.color, ann.strokeWidth);
                    pen.SetStartCap(LineCapFlat);
                    pen.SetEndCap(LineCapFlat);
                    g.DrawLine(&pen, lineStartX, lineStartY,
                               sx + shaftEndDist * std::cos(angle),
                               sy + shaftEndDist * std::sin(angle));
                }

                PointF tri[3] = {
                    PointF(ex, ey),
                    PointF(ex - headLen * std::cos(angle - headAngle), ey - headLen * std::sin(angle - headAngle)),
                    PointF(ex - headLen * std::cos(angle + headAngle), ey - headLen * std::sin(angle + headAngle))
                };
                SolidBrush headBrush(ann.color);
                g.FillPolygon(&headBrush, tri, 3);
            }

            // If White color is selected, change Numbering Arrow number color and circle border to Black so it is clearly visible
            bool isWhiteSelected = (ann.color.GetR() > 235 && ann.color.GetG() > 235 && ann.color.GetB() > 235);
            Color contrastCol = isWhiteSelected ? Color(255, 0, 0, 0) : Color(255, 255, 255, 255);

            SolidBrush fill(ann.color);
            g.FillEllipse(&fill, sx - r, sy - r, r * 2.0f, r * 2.0f);
            Pen ring(contrastCol, 2.2f);
            g.DrawEllipse(&ring, sx - r, sy - r, r * 2.0f, r * 2.0f);

            FontFamily ff(L"Segoe UI");
            Font font(&ff, r * 0.95f, FontStyleBold, UnitPixel);
            StringFormat sf;
            sf.SetAlignment(StringAlignmentCenter);
            sf.SetLineAlignment(StringAlignmentCenter);
            SolidBrush fg(contrastCol);
            std::wstring s = std::to_wstring(ann.stepNumber);
            RectF rc(sx - r, sy - r, r * 2.0f, r * 2.0f);
            g.DrawString(s.c_str(), -1, &font, rc, &sf, &fg);
            break;
        }

        case OverlayTool::Rectangle: {
            float x = std::min(sx, ex), y = std::min(sy, ey);
            float w = std::abs(ex - sx), h = std::abs(ey - sy);
            Pen pen(ann.color, ann.strokeWidth);
            pen.SetLineJoin(LineJoinRound);
            g.DrawRectangle(&pen, x, y, w, h);
            break;
        }

        case OverlayTool::Ellipse: {
            float x = std::min(sx, ex), y = std::min(sy, ey);
            float w = std::abs(ex - sx), h = std::abs(ey - sy);
            Pen pen(ann.color, ann.strokeWidth);
            g.DrawEllipse(&pen, x, y, w, h);
            break;
        }

        case OverlayTool::TextBox: {
            float fSize = AnnotationFontSize(ann.strokeWidth);
            float lineStep = fSize * 1.32f;
            FontFamily ff(AnnotationFontFace());
            Font font(&ff, fSize, kAnnotationFontStyle, UnitPixel);
            const StringFormat* typoSf = StringFormat::GenericTypographic();

            if (ann.text.empty() && !isEditingCaret) break;

            std::vector<std::pair<size_t, std::wstring>> lines;
            {
                size_t start = 0;
                for (size_t i = 0; i < ann.text.size(); ++i) {
                    if (ann.text[i] == L'\n') {
                        lines.push_back({ start, ann.text.substr(start, i - start) });
                        start = i + 1;
                    }
                }
                lines.push_back({ start, ann.text.substr(start) });
            }

            float bx = std::min(sx, ex);
            float by = std::min(sy, ey);
            float maxLineW = 0.0f;
            if (ann.text.empty()) {
                maxLineW = MeasureMonoPrefixWidth(g, font, L"Type here...", 12);
            } else {
                for (const auto& ln : lines) {
                    float w = MeasureMonoPrefixWidth(g, font, ln.second, ln.second.size());
                    if (w > maxLineW) maxLineW = w;
                }
            }

            float boxW = isEditingCaret ? std::max(maxLineW + 14.0f, 110.0f) : std::max(12.0f, maxLineW + 10.0f);
            float boxH = std::max((float)lines.size() * lineStep + 8.0f, fSize + 10.0f);

            if (isEditingCaret || isSelected) {
                SolidBrush editBg(Color(165, 15, 23, 42));
                g.FillRectangle(&editBg, bx - 5.0f, by - 4.0f, boxW, boxH);
                Pen dashPen(ann.color, 1.5f);
                dashPen.SetDashStyle(DashStyleDash);
                g.DrawRectangle(&dashPen, bx - 5.0f, by - 4.0f, boxW, boxH);
            }

            if (isEditingCaret && ann.text.empty()) {
                SolidBrush placeholderFg(Color(145, 203, 213, 225));
                g.DrawString(L"Type here...", -1, &font, PointF(bx, by), typoSf, &placeholderFg);
                bool blinkOn = ((GetTickCount() / 450) % 2) == 0;
                if (blinkOn) {
                    Pen caretPen(Color(255, 56, 189, 248), 2.0f);
                    g.DrawLine(&caretPen, bx, by, bx, by + fSize * 1.18f);
                }
                break;
            }

            if (isEditingCaret && g_Daemon.textCaretPos != g_Daemon.textSelAnchor && !ann.text.empty()) {
                size_t s0 = std::min(g_Daemon.textCaretPos, g_Daemon.textSelAnchor);
                size_t s1 = std::max(g_Daemon.textCaretPos, g_Daemon.textSelAnchor);
                SolidBrush selHighlight(Color(200, 2, 132, 199));
                for (size_t i = 0; i < lines.size(); ++i) {
                    size_t lStart = lines[i].first;
                    size_t lEnd = lStart + lines[i].second.size();
                    if (s1 > lStart && s0 < lEnd) {
                        size_t c0 = std::max(s0, lStart) - lStart;
                        size_t c1 = std::min(s1, lEnd) - lStart;
                        float x0 = bx + MeasureMonoPrefixWidth(g, font, lines[i].second, c0);
                        float x1 = bx + MeasureMonoPrefixWidth(g, font, lines[i].second, c1);
                        g.FillRectangle(&selHighlight, x0, by + (float)i * lineStep, std::max(2.0f, x1 - x0), lineStep);
                    }
                }
            }

            SolidBrush fg(ann.color);
            // Each line is stroked along its glyph outlines and then filled over:
            // the fill covers the inner half of the stroke, so exactly
            // AnnotationOutlineWidth() of it stays showing outside the
            // letterforms. Round joins and caps keep corners and stem ends from
            // growing spikes at these widths.
            Pen outlinePen(AnnotationOutlineColor(ann.color), AnnotationOutlineWidth(fSize) * 2.0f);
            outlinePen.SetLineJoin(LineJoinRound);
            outlinePen.SetStartCap(LineCapRound);
            outlinePen.SetEndCap(LineCapRound);
            for (size_t i = 0; i < lines.size(); ++i) {
                if (lines[i].second.empty()) continue;
                // Non-zero winding is the rule TrueType contours are defined
                // with, so counters ("o", "e") stay open in every glyph.
                GraphicsPath linePath(FillModeWinding);
                if (linePath.AddString(lines[i].second.c_str(), -1, &ff, kAnnotationFontStyle,
                                       fSize, PointF(bx, by + (float)i * lineStep), typoSf) != Ok) {
                    continue;
                }
                g.DrawPath(&outlinePen, &linePath);
                g.FillPath(&fg, &linePath);
            }

            if (isEditingCaret) {
                bool blinkOn = ((GetTickCount() / 450) % 2) == 0;
                if (blinkOn) {
                    size_t cPos = std::min(g_Daemon.textCaretPos, ann.text.size());
                    size_t caretLine = 0, caretCol = 0;
                    for (size_t i = 0; i < lines.size(); ++i) {
                        size_t lStart = lines[i].first;
                        size_t lEnd = lStart + lines[i].second.size();
                        if (cPos >= lStart && cPos <= lEnd) {
                            caretLine = i;
                            caretCol = cPos - lStart;
                            break;
                        }
                    }
                    float cx = bx + MeasureMonoPrefixWidth(g, font, lines[caretLine].second, caretCol);
                    float cy = by + (float)caretLine * lineStep;
                    Pen caretPen(Color(255, 56, 189, 248), 2.0f);
                    g.DrawLine(&caretPen, cx, cy, cx, cy + fSize * 1.18f);
                }
            }
            break;
        }

        default:
            break;
    }

    if (isSelected && ann.tool != OverlayTool::TextBox) {
        Pen selPen(Color(255, 56, 189, 248), 1.5f);
        selPen.SetDashStyle(DashStyleDash);
        float x = std::min(sx, ex) - 6.0f;
        float y = std::min(sy, ey) - 6.0f;
        float w = std::max(16.0f, std::abs(ex - sx) + 12.0f);
        float h = std::max(16.0f, std::abs(ey - sy) + 12.0f);
        g.DrawRectangle(&selPen, x, y, w, h);

        // Endpoint handles for lines, arrows, and number-arrows
        SolidBrush handleFill(Color(255, 255, 255, 255));
        Pen handleBorder(Color(255, 56, 189, 248), 1.5f);
        g.FillEllipse(&handleFill, sx - 5.0f, sy - 5.0f, 10.0f, 10.0f);
        g.DrawEllipse(&handleBorder, sx - 5.0f, sy - 5.0f, 10.0f, 10.0f);
        g.FillEllipse(&handleFill, ex - 5.0f, ey - 5.0f, 10.0f, 10.0f);
        g.DrawEllipse(&handleBorder, ex - 5.0f, ey - 5.0f, 10.0f, 10.0f);
    }
}

void PepperSnapDaemon::RenderHighlighterLayer(Graphics& destG, int regionX, int regionY, int regionW, int regionH, const std::vector<Annotation>& anns, const Annotation* draft, bool nonStacking) {
    bool hasHi = (draft && draft->tool == OverlayTool::Highlighter);
    if (!hasHi) {
        for (const auto& a : anns) {
            if (a.tool == OverlayTool::Highlighter) { hasHi = true; break; }
        }
    }
    if (!hasHi || regionW <= 0 || regionH <= 0) return;

    if (!nonStacking) {
        for (const auto& a : anns) {
            if (a.tool == OverlayTool::Highlighter) {
                DrawVectorAnnotation(destG, a, regionX, regionY, false, false, false);
            }
        }
        if (draft && draft->tool == OverlayTool::Highlighter) {
            DrawVectorAnnotation(destG, *draft, regionX, regionY, false, false, false);
        }
        return;
    }

    // Non-Stacking Highlighter: render all highlighter strokes at 100% opaque onto a tight-bounds offscreen layer,
    //    then composite the tight layer once at uniform 37% opacity so overlapping strokes never darken!
    float minX = (float)(regionX + regionW);
    float minY = (float)(regionY + regionH);
    float maxX = (float)regionX;
    float maxY = (float)regionY;

    auto expandBounds = [&](const Annotation& a) {
        float pad = std::max(16.0f, a.strokeWidth * 3.0f + 8.0f);
        if (!a.points.empty()) {
            for (const auto& pt : a.points) {
                minX = std::min(minX, pt.x - pad);
                minY = std::min(minY, pt.y - pad);
                maxX = std::max(maxX, pt.x + pad);
                maxY = std::max(maxY, pt.y + pad);
            }
        } else {
            minX = std::min(minX, std::min(a.startPt.x, a.endPt.x) - pad);
            minY = std::min(minY, std::min(a.startPt.y, a.endPt.y) - pad);
            maxX = std::max(maxX, std::max(a.startPt.x, a.endPt.x) + pad);
            maxY = std::max(maxY, std::max(a.startPt.y, a.endPt.y) + pad);
        }
    };

    for (const auto& a : anns) {
        if (a.tool == OverlayTool::Highlighter) expandBounds(a);
    }
    if (draft && draft->tool == OverlayTool::Highlighter) {
        expandBounds(*draft);
    }

    int bLeft   = std::max(regionX, (int)std::floor(minX));
    int bTop    = std::max(regionY, (int)std::floor(minY));
    int bRight  = std::min(regionX + regionW, (int)std::ceil(maxX));
    int bBottom = std::min(regionY + regionH, (int)std::ceil(maxY));
    int bW = bRight - bLeft;
    int bH = bBottom - bTop;
    if (bW <= 0 || bH <= 0) return;

    static Bitmap* s_hiLayer = nullptr;
    static int s_hiCapW = 0;
    static int s_hiCapH = 0;
    if (!s_hiLayer || bW > s_hiCapW || bH > s_hiCapH) {
        delete s_hiLayer;
        s_hiCapW = std::max(bW, 256);
        s_hiCapH = std::max(bH, 256);
        s_hiLayer = new Bitmap(s_hiCapW, s_hiCapH, PixelFormat32bppARGB);
    }
    Graphics hg(s_hiLayer);
    hg.SetCompositingMode(CompositingModeSourceCopy);
    SolidBrush clearBrush(Color(0, 0, 0, 0));
    hg.FillRectangle(&clearBrush, 0, 0, bW, bH);
    hg.SetCompositingMode(CompositingModeSourceOver);
    hg.SetSmoothingMode(SmoothingModeAntiAlias);

    for (const auto& a : anns) {
        if (a.tool == OverlayTool::Highlighter) {
            DrawVectorAnnotation(hg, a, bLeft, bTop, false, false, true);
        }
    }
    if (draft && draft->tool == OverlayTool::Highlighter) {
        DrawVectorAnnotation(hg, *draft, bLeft, bTop, false, false, true);
    }

    ColorMatrix cm = {
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 95.0f / 255.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 1.0f
    };
    ImageAttributes ia;
    ia.SetColorMatrix(&cm, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);
    Rect destRc(bLeft - regionX, bTop - regionY, bW, bH);
    destG.DrawImage(s_hiLayer, destRc, 0, 0, bW, bH, UnitPixel, &ia);
}

void PepperSnapDaemon::RecalcNextStepNum() {
    int maxStep = 0;
    for (const auto& a : annotations) {
        if (a.tool == OverlayTool::NumberArrow && a.stepNumber > maxStep) {
            maxStep = a.stepNumber;
        }
    }
    nextStepNum = maxStep + 1;
}

void PepperSnapDaemon::CommitActiveTextBox() {
    if (!isEditingText) return;
    isEditingText = false;
    isDraggingTextBoxSel = false;
    textCaretPos = textSelAnchor = 0;
    if (!editingTextAnn.text.empty()) {
        PushUndo();
        float fSize = AnnotationFontSize(editingTextAnn.strokeWidth);
        float lineStep = fSize * 1.32f;
        FontFamily ff(AnnotationFontFace());
        Font font(&ff, fSize, kAnnotationFontStyle, UnitPixel);
        HDC hdc = GetDC(nullptr);
        Graphics g(hdc);
        float maxW = 0.0f;
        int lineCount = 1;
        size_t start = 0;
        for (size_t i = 0; i <= editingTextAnn.text.size(); ++i) {
            if (i == editingTextAnn.text.size() || editingTextAnn.text[i] == L'\n') {
                std::wstring ln = editingTextAnn.text.substr(start, i - start);
                float w = MeasureMonoPrefixWidth(g, font, ln, ln.size());
                if (w > maxW) maxW = w;
                if (i < editingTextAnn.text.size()) lineCount++;
                start = i + 1;
            }
        }
        ReleaseDC(nullptr, hdc);
        float bx = std::min(editingTextAnn.startPt.x, editingTextAnn.endPt.x);
        float by = std::min(editingTextAnn.startPt.y, editingTextAnn.endPt.y);
        editingTextAnn.startPt = { bx, by };
        editingTextAnn.endPt = { bx + std::max(maxW + 10.0f, 12.0f), by + lineCount * lineStep + 6.0f };
        annotations.push_back(editingTextAnn);
    }
    editingTextAnn = Annotation();
}

RECT PepperSnapDaemon::GetEditingTextBoxRect() const {
    float bx = std::min(editingTextAnn.startPt.x, editingTextAnn.endPt.x);
    float by = std::min(editingTextAnn.startPt.y, editingTextAnn.endPt.y);
    float fSize = AnnotationFontSize(editingTextAnn.strokeWidth);
    float lineStep = fSize * 1.32f;
    FontFamily ff(AnnotationFontFace());
    Font font(&ff, fSize, kAnnotationFontStyle, UnitPixel);
    HDC hdc = GetDC(nullptr);
    Graphics g(hdc);
    float maxW = 0.0f;
    int lineCount = 1;
    if (editingTextAnn.text.empty()) {
        maxW = MeasureMonoPrefixWidth(g, font, L"Type here...", 12);
    } else {
        size_t start = 0;
        for (size_t i = 0; i <= editingTextAnn.text.size(); ++i) {
            if (i == editingTextAnn.text.size() || editingTextAnn.text[i] == L'\n') {
                std::wstring ln = editingTextAnn.text.substr(start, i - start);
                float w = MeasureMonoPrefixWidth(g, font, ln, ln.size());
                if (w > maxW) maxW = w;
                if (i < editingTextAnn.text.size()) lineCount++;
                start = i + 1;
            }
        }
    }
    ReleaseDC(nullptr, hdc);
    float boxW = std::max(maxW + 14.0f, 110.0f);
    float boxH = std::max(lineCount * lineStep + 8.0f, fSize + 10.0f);
    return { (LONG)(bx - 6.0f), (LONG)(by - 5.0f), (LONG)(bx + boxW + 6.0f), (LONG)(by + boxH + 5.0f) };
}

size_t PepperSnapDaemon::HitTestTextBoxIndex(int mouseX, int mouseY) const {
    const std::wstring& s = editingTextAnn.text;
    if (s.empty()) return 0;
    float bx = std::min(editingTextAnn.startPt.x, editingTextAnn.endPt.x);
    float by = std::min(editingTextAnn.startPt.y, editingTextAnn.endPt.y);
    float fSize = AnnotationFontSize(editingTextAnn.strokeWidth);
    float lineStep = fSize * 1.32f;

    std::vector<std::pair<size_t, std::wstring>> lines;
    size_t start = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'\n') {
            lines.push_back({ start, s.substr(start, i - start) });
            start = i + 1;
        }
    }
    lines.push_back({ start, s.substr(start) });

    int lineIdx = (int)std::floor(((float)mouseY - by) / lineStep);
    lineIdx = std::max(0, std::min((int)lines.size() - 1, lineIdx));

    const std::wstring& ln = lines[lineIdx].second;
    float relX = (float)mouseX - bx;
    if (relX <= 0.0f || ln.empty()) return lines[lineIdx].first;

    FontFamily ff(AnnotationFontFace());
    Font font(&ff, fSize, kAnnotationFontStyle, UnitPixel);
    HDC hdc = GetDC(nullptr);
    Graphics g(hdc);
    size_t col = ln.size();
    float prevX = 0.0f;
    for (size_t i = 0; i < ln.size(); ++i) {
        float nextX = MeasureMonoPrefixWidth(g, font, ln, i + 1);
        if (relX < (prevX + nextX) * 0.5f) {
            col = i;
            break;
        }
        prevX = nextX;
    }
    ReleaseDC(nullptr, hdc);
    return lines[lineIdx].first + col;
}

void PepperSnapDaemon::ApplyTextBoxKeyDown(WPARAM wParam, bool ctrl, bool shift, HWND hWnd) {
    std::wstring& text = editingTextAnn.text;
    textCaretPos = std::min(textCaretPos, text.size());
    textSelAnchor = std::min(textSelAnchor, text.size());

    auto deleteSelection = [&]() -> bool {
        if (textCaretPos == textSelAnchor) return false;
        size_t s0 = std::min(textCaretPos, textSelAnchor);
        size_t s1 = std::max(textCaretPos, textSelAnchor);
        text.erase(s0, s1 - s0);
        textCaretPos = textSelAnchor = s0;
        return true;
    };

    if (ctrl) {
        if (wParam == 'A') {
            textSelAnchor = 0;
            textCaretPos = text.size();
            return;
        }
        if ((wParam == 'C' || wParam == 'X') && textCaretPos != textSelAnchor) {
            size_t s0 = std::min(textCaretPos, textSelAnchor);
            size_t s1 = std::max(textCaretPos, textSelAnchor);
            std::wstring sub = text.substr(s0, s1 - s0);
            if (OpenClipboard(hWnd)) {
                EmptyClipboard();
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (sub.size() + 1) * sizeof(wchar_t));
                if (hMem) {
                    wchar_t* dst = (wchar_t*)GlobalLock(hMem);
                    if (dst) {
                        memcpy(dst, sub.c_str(), (sub.size() + 1) * sizeof(wchar_t));
                        GlobalUnlock(hMem);
                        SetClipboardData(CF_UNICODETEXT, hMem);
                    }
                }
                CloseClipboard();
            }
            if (wParam == 'X') deleteSelection();
            return;
        }
        if (wParam == 'V') {
            if (OpenClipboard(hWnd)) {
                HANDLE hData = GetClipboardData(CF_UNICODETEXT);
                if (hData) {
                    const wchar_t* psz = (const wchar_t*)GlobalLock(hData);
                    if (psz) {
                        deleteSelection();
                        for (size_t i = 0; psz[i] != 0; ++i) {
                            wchar_t c = psz[i];
                            if (c == L'\r') continue;
                            if (c >= 32 || c == L'\n') {
                                text.insert(textCaretPos, 1, c);
                                textCaretPos++;
                            }
                        }
                        textSelAnchor = textCaretPos;
                        GlobalUnlock(hData);
                    }
                }
                CloseClipboard();
            }
            return;
        }
    }

    std::vector<std::pair<size_t, size_t>> lineSpans;
    {
        size_t start = 0;
        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] == L'\n') {
                lineSpans.push_back({ start, i - start });
                start = i + 1;
            }
        }
        lineSpans.push_back({ start, text.size() - start });
    }
    size_t curLine = 0, curCol = 0;
    for (size_t i = 0; i < lineSpans.size(); ++i) {
        if (textCaretPos >= lineSpans[i].first && textCaretPos <= lineSpans[i].first + lineSpans[i].second) {
            curLine = i;
            curCol = textCaretPos - lineSpans[i].first;
            break;
        }
    }

    switch (wParam) {
        case VK_LEFT:
            if (!shift && textCaretPos != textSelAnchor) {
                textCaretPos = textSelAnchor = std::min(textCaretPos, textSelAnchor);
            } else {
                if (textCaretPos > 0) textCaretPos--;
                if (!shift) textSelAnchor = textCaretPos;
            }
            break;
        case VK_RIGHT:
            if (!shift && textCaretPos != textSelAnchor) {
                textCaretPos = textSelAnchor = std::max(textCaretPos, textSelAnchor);
            } else {
                if (textCaretPos < text.size()) textCaretPos++;
                if (!shift) textSelAnchor = textCaretPos;
            }
            break;
        case VK_HOME:
            textCaretPos = lineSpans[curLine].first;
            if (!shift) textSelAnchor = textCaretPos;
            break;
        case VK_END:
            textCaretPos = lineSpans[curLine].first + lineSpans[curLine].second;
            if (!shift) textSelAnchor = textCaretPos;
            break;
        case VK_UP:
            if (curLine > 0) {
                textCaretPos = lineSpans[curLine - 1].first + std::min(curCol, lineSpans[curLine - 1].second);
            } else {
                textCaretPos = 0;
            }
            if (!shift) textSelAnchor = textCaretPos;
            break;
        case VK_DOWN:
            if (curLine + 1 < lineSpans.size()) {
                textCaretPos = lineSpans[curLine + 1].first + std::min(curCol, lineSpans[curLine + 1].second);
            } else {
                textCaretPos = text.size();
            }
            if (!shift) textSelAnchor = textCaretPos;
            break;
        case VK_BACK:
            if (!deleteSelection() && textCaretPos > 0) {
                text.erase(textCaretPos - 1, 1);
                textCaretPos--;
                textSelAnchor = textCaretPos;
            }
            break;
        case VK_DELETE:
            if (!deleteSelection() && textCaretPos < text.size()) {
                text.erase(textCaretPos, 1);
            }
            break;
        default:
            break;
    }
}

void PepperSnapDaemon::ApplyStrokeToSelectedAnnotation(float newStroke, bool recordUndo) {
    newStroke = std::max(1.0f, std::min(120.0f, newStroke));
    activeStroke = newStroke;
    SaveSettings();
    if (isEditingText) {
        editingTextAnn.strokeWidth = newStroke;
    }
    if (!HasSelectedAnnotations()) return;
    bool pushed = false;
    for (auto& a : annotations) {
        if (IsAnnotationSelected(a.id)) {
            if (recordUndo && !pushed && std::abs(a.strokeWidth - newStroke) > 0.05f) {
                PushUndo();
                pushed = true;
            }
            a.strokeWidth = newStroke;
            if (a.tool == OverlayTool::TextBox) {
                FontFamily ff(AnnotationFontFace());
                float fSize = AnnotationFontSize(a.strokeWidth);
                Font font(&ff, fSize, kAnnotationFontStyle, UnitPixel);
                HDC hdc = GetDC(nullptr);
                Graphics g(hdc);
                RectF bounds;
                g.MeasureString(a.text.c_str(), -1, &font, PointF(a.startPt.x, a.startPt.y), &bounds);
                ReleaseDC(nullptr, hdc);
                a.endPt = { a.startPt.x + bounds.Width + 12.0f, a.startPt.y + bounds.Height + 8.0f };
            }
        }
    }
}

void PepperSnapDaemon::CommitActiveStrokeInput() {
    if (!isEditingStroke) return;
    isEditingStroke = false;
    isDraggingStrokeText = false;
    std::wstring s = editingStrokeText;
    editingStrokeText.clear();
    strokeCaretPos = strokeSelAnchor = 0;
    for (wchar_t& c : s) {
        if ((c < L'0' || c > L'9') && c != L'.') c = L' ';
    }
    std::wstringstream ss(s);
    float val = 0.0f;
    if ((ss >> val) && val >= 1.0f) {
        ApplyStrokeToSelectedAnnotation(std::max(1.0f, std::min(120.0f, val)), true);
    }
}

void PepperSnapDaemon::CommitActiveSizeInput() {
    if (!isEditingSize) return;
    isEditingSize = false;
    isDraggingSizeText = false;
    std::wstring raw = editingSizeText;
    editingSizeText.clear();
    sizeCaretPos = sizeSelAnchor = 0;

    int curW = std::max(1, (int)std::abs(selRect.right - selRect.left));
    int curH = std::max(1, (int)std::abs(selRect.bottom - selRect.top));
    int newW = 0, newH = 0;
    bool parsed = false;

    size_t sep = raw.find_first_of(L"xX\x00D7*,");
    auto hasDigit = [](const std::wstring& str) {
        for (wchar_t c : str) {
            if (c >= L'0' && c <= L'9') return true;
        }
        return false;
    };

    if (sep != std::wstring::npos) {
        std::wstring leftStr  = raw.substr(0, sep);
        std::wstring rightStr = raw.substr(sep + 1);
        bool hasLeft  = hasDigit(leftStr);
        bool hasRight = hasDigit(rightStr);
        if (hasLeft && !hasRight) {
            try {
                int wVal = std::stoi(leftStr);
                if (wVal >= 1) {
                    newW = wVal;
                    if (hasCustomRatio && customRatioW > 0 && customRatioH > 0) {
                        newH = std::max(1, (int)std::round((double)wVal * (double)customRatioH / (double)customRatioW));
                    } else {
                        newH = hasSelection ? curH : wVal;
                    }
                    parsed = true;
                }
            } catch (...) {}
        } else if (!hasLeft && hasRight) {
            try {
                size_t lastSep = rightStr.find_last_of(L"xX\x00D7*, ");
                std::wstring hDigits = (lastSep != std::wstring::npos) ? rightStr.substr(lastSep + 1) : rightStr;
                int hVal = std::stoi(hDigits);
                if (hVal >= 1) {
                    newH = hVal;
                    if (hasCustomRatio && customRatioW > 0 && customRatioH > 0) {
                        newW = std::max(1, (int)std::round((double)hVal * (double)customRatioW / (double)customRatioH));
                    } else {
                        newW = hasSelection ? curW : hVal;
                    }
                    parsed = true;
                }
            } catch (...) {}
        }
    }

    if (!parsed) {
        std::wstring s = raw;
        for (wchar_t& c : s) {
            if (c < L'0' || c > L'9') c = L' ';
        }
        std::wstringstream ss(s);
        if ((ss >> newW) && newW >= 1) {
            if (!(ss >> newH) || newH < 1) {
                if (hasCustomRatio && customRatioW > 0 && customRatioH > 0) {
                    newH = std::max(1, (int)std::round((double)newW * (double)customRatioH / (double)customRatioW));
                } else {
                    newH = hasSelection ? curH : newW;
                }
            } else if (hasCustomRatio && customRatioW > 0 && customRatioH > 0) {
                if (newW != curW && newH == curH) {
                    newH = std::max(1, (int)std::round((double)newW * (double)customRatioH / (double)customRatioW));
                } else if (newH != curH && newW == curW) {
                    newW = std::max(1, (int)std::round((double)newH * (double)customRatioW / (double)customRatioH));
                } else {
                    int candH = std::max(1, (int)std::round((double)newW * (double)customRatioH / (double)customRatioW));
                    if (candH <= newH) {
                        newH = candH;
                    } else {
                        newW = std::max(1, (int)std::round((double)newH * (double)customRatioW / (double)customRatioH));
                    }
                }
            }
            parsed = true;
        }
    }

    if (parsed && newW >= 1 && newH >= 1) {
        int sx = std::min(selRect.left, selRect.right);
        int sy = std::min(selRect.top, selRect.bottom);
        int maxW = std::max(1, vScreenW - sx);
        int maxH = std::max(1, vScreenH - sy);
        if (hasCustomRatio && customRatioW > 0 && customRatioH > 0) {
            if (newW > maxW) {
                newW = maxW;
                newH = std::max(1, (int)std::round((double)newW * (double)customRatioH / (double)customRatioW));
            }
            if (newH > maxH) {
                newH = maxH;
                newW = std::max(1, (int)std::round((double)newH * (double)customRatioW / (double)customRatioH));
            }
        }
        newW = std::min(maxW, std::max(1, newW));
        newH = std::min(maxH, std::max(1, newH));
        RECT prevRect = { sx, sy, sx + curW, sy + curH };
        RECT nextRect = { sx, sy, sx + newW, sy + newH };
        if (hasCustomQuad) {
            POINT prevQuad[4];
            GetSelectionQuadPoints(prevQuad);
            ScaleQuadFromOrigRect(prevQuad, prevRect, nextRect);
        } else {
            selRect = nextRect;
            SyncQuadFromSelRect();
        }
        hasSelection = true;
        hasCustomHudPos = false;
        activeSelectionRatioW = 0;
        activeSelectionRatioH = 0;
    }
}

void PepperSnapDaemon::CommitActiveRatioInput() {
    if (!isEditingRatio) return;
    isEditingRatio = false;
    isDraggingRatioText = false;
    std::wstring raw = editingRatioText;
    editingRatioText.clear();
    ratioCaretPos = ratioSelAnchor = 0;

    int curW = std::max(1, (int)std::abs(selRect.right - selRect.left));
    int curH = std::max(1, (int)std::abs(selRect.bottom - selRect.top));
    int gCur = ComputeIntGcd(curW, curH);
    int curRW = (hasCustomRatio && customRatioW > 0) ? customRatioW : (curW / gCur);
    int curRH = (hasCustomRatio && customRatioH > 0) ? customRatioH : (curH / gCur);

    int newRW = 0, newRH = 0;
    bool parsed = false;

    size_t sep = raw.find_first_of(L":xX\x00D7*,/");
    auto hasDigit = [](const std::wstring& str) {
        for (wchar_t c : str) {
            if (c >= L'0' && c <= L'9') return true;
        }
        return false;
    };

    if (sep != std::wstring::npos) {
        std::wstring leftStr  = raw.substr(0, sep);
        std::wstring rightStr = raw.substr(sep + 1);
        bool hasLeft  = hasDigit(leftStr);
        bool hasRight = hasDigit(rightStr);
        if (hasLeft && !hasRight) {
            try {
                int wVal = std::stoi(leftStr);
                if (wVal >= 1) {
                    newRW = wVal;
                    newRH = hasSelection ? curRH : wVal;
                    parsed = true;
                }
            } catch (...) {}
        } else if (!hasLeft && hasRight) {
            try {
                size_t lastSep = rightStr.find_last_of(L":xX\x00D7*,/ ");
                std::wstring hDigits = (lastSep != std::wstring::npos) ? rightStr.substr(lastSep + 1) : rightStr;
                int hVal = std::stoi(hDigits);
                if (hVal >= 1) {
                    newRH = hVal;
                    newRW = hasSelection ? curRW : hVal;
                    parsed = true;
                }
            } catch (...) {}
        }
    }

    if (!parsed) {
        std::wstring s = raw;
        for (wchar_t& c : s) {
            if (c < L'0' || c > L'9') c = L' ';
        }
        std::wstringstream ss(s);
        if ((ss >> newRW) && newRW >= 1) {
            if (!(ss >> newRH) || newRH < 1) {
                newRH = hasSelection ? curRH : newRW;
            }
            parsed = true;
        }
    }

    if (parsed && newRW >= 1 && newRH >= 1) {
        customRatioW = newRW;
        customRatioH = newRH;
        hasCustomRatio = true;
        activeSelectionRatioW = 0;
        activeSelectionRatioH = 0;

        int sx = std::min(selRect.left, selRect.right);
        int sy = std::min(selRect.top, selRect.bottom);
        int maxW = std::max(1, vScreenW - sx);
        int maxH = std::max(1, vScreenH - sy);

        int newW = curW;
        int newH = std::max(1, (int)std::round((double)newW * (double)customRatioH / (double)customRatioW));
        if (newW > maxW) {
            newW = maxW;
            newH = std::max(1, (int)std::round((double)newW * (double)customRatioH / (double)customRatioW));
        }
        if (newH > maxH) {
            newH = maxH;
            newW = std::max(1, (int)std::round((double)newH * (double)customRatioW / (double)customRatioH));
        }
        newW = std::min(maxW, std::max(1, newW));
        newH = std::min(maxH, std::max(1, newH));
        RECT prevRect = { sx, sy, sx + curW, sy + curH };
        RECT nextRect = { sx, sy, sx + newW, sy + newH };
        if (hasCustomQuad) {
            POINT prevQuad[4];
            GetSelectionQuadPoints(prevQuad);
            ScaleQuadFromOrigRect(prevQuad, prevRect, nextRect);
        } else {
            selRect = nextRect;
            SyncQuadFromSelRect();
        }
        hasSelection = true;
        hasCustomHudPos = false;
    }
}

float PepperSnapDaemon::MeasureMonoPrefixWidth(Graphics& g, const Font& monoFont, const std::wstring& s, size_t count) {
    if (count == 0 || s.empty()) return 0.0f;
    size_t n = std::min(count, s.size());
    StringFormat sf(StringFormat::GenericTypographic());
    sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsMeasureTrailingSpaces);
    CharacterRange cr(0, (INT)n);
    sf.SetMeasurableCharacterRanges(1, &cr);
    Region rgn;
    if (g.MeasureCharacterRanges(s.c_str(), (INT)s.size(), &monoFont, RectF(0.0f, 0.0f, 4000.0f, 1000.0f), &sf, 1, &rgn) == Ok) {
        RectF bounds;
        if (rgn.GetBounds(&bounds, &g) == Ok && bounds.Width > 0.0f) {
            return std::max(0.0f, bounds.X + bounds.Width);
        }
    }
    RectF r1, r0;
    std::wstring wrapped = L"|" + s.substr(0, n) + L"|";
    g.MeasureString(wrapped.c_str(), -1, &monoFont, PointF(0, 0), &sf, &r1);
    g.MeasureString(L"||", -1, &monoFont, PointF(0, 0), &sf, &r0);
    return std::max(0.0f, r1.Width - r0.Width);
}

float PepperSnapDaemon::GetFittedPillFontSize(Graphics& g, const FontFamily& ff, const std::wstring& fullText, int boxW, int boxH) {
    float fontSize = std::max(11.0f, (float)boxH * 0.60f); // 16.8px for 28px-tall toolbar box
    float maxTextW = std::max(12.0f, (float)(boxW - 10));
    if (!fullText.empty()) {
        Font testFont(&ff, fontSize, FontStyleRegular, UnitPixel);
        float measuredW = MeasureMonoPrefixWidth(g, testFont, fullText, fullText.size());
        if (measuredW > maxTextW && measuredW > 0.1f) {
            fontSize = std::max(9.5f, fontSize * (maxTextW / measuredW));
            for (int iter = 0; iter < 8; ++iter) {
                Font f2(&ff, fontSize, FontStyleRegular, UnitPixel);
                float w2 = MeasureMonoPrefixWidth(g, f2, fullText, fullText.size());
                if (w2 <= maxTextW || fontSize <= 9.5f) break;
                fontSize = std::max(9.5f, fontSize - 0.3f);
            }
        }
    }
    return fontSize;
}

float PepperSnapDaemon::GetCenteredPillTextLeftX(Graphics& g, const Font& font, const std::wstring& fullText, int boxLeft, int boxW) {
    if (fullText.empty()) return (float)boxLeft + (float)boxW * 0.5f;
    StringFormat sf(StringFormat::GenericTypographic());
    sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsMeasureTrailingSpaces);
    CharacterRange cr(0, (INT)fullText.size());
    sf.SetMeasurableCharacterRanges(1, &cr);
    Region rgn;
    if (g.MeasureCharacterRanges(fullText.c_str(), (INT)fullText.size(), &font, RectF(0.0f, 0.0f, 4000.0f, 1000.0f), &sf, 1, &rgn) == Ok) {
        RectF bounds;
        if (rgn.GetBounds(&bounds, &g) == Ok && bounds.Width > 0.0f) {
            float midX = bounds.X + bounds.Width * 0.5f;
            return (float)boxLeft + (float)boxW * 0.5f - midX;
        }
    }
    float fullW = MeasureMonoPrefixWidth(g, font, fullText, fullText.size());
    return (float)boxLeft + ((float)boxW - fullW) * 0.5f;
}

size_t PepperSnapDaemon::HitTestMonoIndex(int mouseX, const RECT& boxRect, const std::wstring& s, const std::wstring& suffix) {
    if (s.empty()) return 0;
    int boxW = std::max(16, (int)(boxRect.right - boxRect.left));
    int boxH = std::max(16, (int)(boxRect.bottom - boxRect.top));
    HDC hdc = GetDC(nullptr);
    Graphics g(hdc);
    FontFamily ff(L"Segoe UI");
    std::wstring fullText = s + suffix;
    float fontSize = GetFittedPillFontSize(g, ff, fullText, boxW, boxH);
    Font pillFont(&ff, fontSize, FontStyleRegular, UnitPixel);
    float textLeftX = GetCenteredPillTextLeftX(g, pillFont, fullText, boxRect.left, boxW);
    if ((float)mouseX <= textLeftX) {
        ReleaseDC(nullptr, hdc);
        return 0;
    }
    float relX = (float)mouseX - textLeftX;
    size_t result = s.size();
    for (size_t i = 0; i < s.size(); ++i) {
        float x0 = MeasureMonoPrefixWidth(g, pillFont, s, i);
        float x1 = MeasureMonoPrefixWidth(g, pillFont, s, i + 1);
        if (relX < (x0 + x1) * 0.5f) {
            result = i;
            break;
        }
    }
    ReleaseDC(nullptr, hdc);
    return result;
}

void PepperSnapDaemon::DrawEditablePillText(
    Graphics& g, const RECT& boxRect,
    const std::wstring& text, const std::wstring& suffix,
    bool isEditing, size_t caretPos, size_t selAnchor,
    const Color* customTextColor
) {
    int boxLeft = boxRect.left;
    int boxTopY = boxRect.top;
    int boxW = std::max(16, (int)(boxRect.right - boxRect.left));
    int boxH = std::max(16, (int)(boxRect.bottom - boxRect.top));

    FontFamily ff(L"Segoe UI");
    std::wstring fullText = text + suffix;
    float fontSize = GetFittedPillFontSize(g, ff, fullText, boxW, boxH);
    Font pillFont(&ff, fontSize, FontStyleRegular, UnitPixel);

    StringFormat sf(StringFormat::GenericTypographic());
    sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsMeasureTrailingSpaces);

    float textLeftX = GetCenteredPillTextLeftX(g, pillFont, fullText, boxLeft, boxW);

    GraphicsPath vPath;
    vPath.AddString(L"0123456789%", -1, &ff, FontStyleRegular, fontSize, PointF(0.0f, 0.0f), &sf);
    RectF vBounds;
    vPath.GetBounds(&vBounds);
    float digitsMidY = (vBounds.Height > 0.1f)
        ? (vBounds.Y + vBounds.Height * 0.5f)
        : (fontSize * 0.72f);
    float textTopY = (float)boxTopY + (float)boxH * 0.5f - digitsMidY;

    Color fgCol = customTextColor ? *customTextColor : Color(255, 248, 250, 252);
    SolidBrush mainBrush(fgCol);
    SolidBrush suffixBrush(isEditing ? Color(255, 148, 163, 184) : fgCol);

    GraphicsState st = g.Save();
    g.SetClip(Rect(boxLeft + 1, boxTopY + 1, std::max(1, boxW - 2), std::max(1, boxH - 2)), CombineModeIntersect);

    if (isEditing && caretPos != selAnchor && !text.empty()) {
        size_t s0 = std::min(caretPos, selAnchor);
        size_t s1 = std::max(caretPos, selAnchor);
        float x0 = textLeftX + MeasureMonoPrefixWidth(g, pillFont, text, s0);
        float x1 = textLeftX + MeasureMonoPrefixWidth(g, pillFont, text, s1);
        SolidBrush selHighlight(Color(200, 2, 132, 199)); // Sky-600 selection highlight
        g.FillRectangle(&selHighlight, x0, (float)(boxTopY + 3), std::max(2.0f, x1 - x0), (float)(boxH - 6));
    }

    if (!isEditing) {
        g.DrawString(fullText.c_str(), -1, &pillFont, PointF(textLeftX, textTopY), &sf, &mainBrush);
    } else {
        g.DrawString(text.c_str(), -1, &pillFont, PointF(textLeftX, textTopY), &sf, &mainBrush);
        float textW = MeasureMonoPrefixWidth(g, pillFont, fullText, text.size());
        g.DrawString(suffix.c_str(), -1, &pillFont, PointF(textLeftX + textW, textTopY), &sf, &suffixBrush);
    }

    if (isEditing) {
        bool blinkOn = ((GetTickCount() / 450) % 2) == 0;
        if (blinkOn) {
            float cx = textLeftX + MeasureMonoPrefixWidth(g, pillFont, text, caretPos);
            Pen caretPen(Color(255, 56, 189, 248), 1.8f);
            g.DrawLine(&caretPen, cx, (float)(boxTopY + 3), cx, (float)(boxTopY + boxH - 3));
        }
    }
    g.Restore(st);
}

void PepperSnapDaemon::ApplyInlineEditChar(wchar_t ch, std::wstring& text, size_t& caret, size_t& anchor, size_t maxLen) {
    caret = std::min(caret, text.size());
    anchor = std::min(anchor, text.size());
    if (caret != anchor) {
        size_t s0 = std::min(caret, anchor);
        size_t s1 = std::max(caret, anchor);
        text.erase(s0, s1 - s0);
        caret = anchor = s0;
    }
    if (text.size() < maxLen) {
        text.insert(caret, 1, ch);
        caret++;
        anchor = caret;
    }
}

void PepperSnapDaemon::ApplyInlineEditKeyDown(
    WPARAM wParam, bool ctrl, bool shift,
    std::wstring& text, size_t& caret, size_t& anchor,
    size_t maxLen, bool isSizeField, HWND hWnd
) {
    caret = std::min(caret, text.size());
    anchor = std::min(anchor, text.size());

    auto deleteSelection = [&]() -> bool {
        if (caret == anchor) return false;
        size_t s0 = std::min(caret, anchor);
        size_t s1 = std::max(caret, anchor);
        text.erase(s0, s1 - s0);
        caret = anchor = s0;
        return true;
    };

    if (ctrl) {
        if (wParam == 'A') {
            anchor = 0;
            caret = text.size();
            return;
        }
        if (wParam == 'C' && caret != anchor) {
            size_t s0 = std::min(caret, anchor);
            size_t s1 = std::max(caret, anchor);
            std::wstring sub = text.substr(s0, s1 - s0);
            if (OpenClipboard(hWnd)) {
                EmptyClipboard();
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (sub.size() + 1) * sizeof(wchar_t));
                if (hMem) {
                    wchar_t* dst = (wchar_t*)GlobalLock(hMem);
                    if (dst) {
                        memcpy(dst, sub.c_str(), (sub.size() + 1) * sizeof(wchar_t));
                        GlobalUnlock(hMem);
                        SetClipboardData(CF_UNICODETEXT, hMem);
                    }
                }
                CloseClipboard();
            }
            return;
        }
        if (wParam == 'V') {
            if (OpenClipboard(hWnd)) {
                HANDLE hData = GetClipboardData(CF_UNICODETEXT);
                if (hData) {
                    const wchar_t* psz = (const wchar_t*)GlobalLock(hData);
                    if (psz) {
                        deleteSelection();
                        for (size_t i = 0; psz[i] != 0 && text.size() < maxLen; ++i) {
                            wchar_t c = psz[i];
                            bool valid = (c >= L'0' && c <= L'9') ||
                                         (isSizeField ? (c == L'x' || c == L'X' || c == L':' || c == L'*' || c == L',' || c == L'/' || c == L' ') : (c == L'.'));
                            if (valid) {
                                if (isSizeField && c == L'X') c = L'x';
                                text.insert(caret, 1, c);
                                caret++;
                            }
                        }
                        anchor = caret;
                        GlobalUnlock(hData);
                    }
                }
                CloseClipboard();
            }
            return;
        }
    }

    switch (wParam) {
        case VK_LEFT:
            if (!shift && caret != anchor) {
                caret = anchor = std::min(caret, anchor);
            } else {
                if (caret > 0) caret--;
                if (!shift) anchor = caret;
            }
            break;
        case VK_RIGHT:
            if (!shift && caret != anchor) {
                caret = anchor = std::max(caret, anchor);
            } else {
                if (caret < text.size()) caret++;
                if (!shift) anchor = caret;
            }
            break;
        case VK_HOME:
        case VK_UP:
            caret = 0;
            if (!shift) anchor = caret;
            break;
        case VK_END:
        case VK_DOWN:
            caret = text.size();
            if (!shift) anchor = caret;
            break;
        case VK_BACK:
            if (!deleteSelection() && caret > 0) {
                text.erase(caret - 1, 1);
                caret--;
                anchor = caret;
            }
            break;
        case VK_DELETE:
            if (!deleteSelection() && caret < text.size()) {
                text.erase(caret, 1);
            }
            break;
        default:
            break;
    }
}

bool PepperSnapDaemon::AnnotationIntersectsRect(const Annotation& a, const RECT& rc) {
    float rL = (float)std::min(rc.left, rc.right);
    float rT = (float)std::min(rc.top, rc.bottom);
    float rR = (float)std::max(rc.left, rc.right);
    float rB = (float)std::max(rc.top, rc.bottom);
    if (rR - rL < 1.0f && rB - rT < 1.0f) return false;

    auto ptInRc = [&](float x, float y, float pad) -> bool {
        return x >= rL - pad && x <= rR + pad && y >= rT - pad && y <= rB + pad;
    };
    auto distSeg = [](float px, float py, float x1, float y1, float x2, float y2) -> float {
        float dx = x2 - x1, dy = y2 - y1;
        float l2 = dx * dx + dy * dy;
        if (l2 == 0.0f) return std::hypot(px - x1, py - y1);
        float t = std::max(0.0f, std::min(1.0f, ((px - x1) * dx + (py - y1) * dy) / l2));
        return std::hypot(px - (x1 + t * dx), py - (y1 + t * dy));
    };
    auto segIntersectsRc = [&](float x1, float y1, float x2, float y2, float pad) -> bool {
        float sMinX = std::min(x1, x2) - pad, sMaxX = std::max(x1, x2) + pad;
        float sMinY = std::min(y1, y2) - pad, sMaxY = std::max(y1, y2) + pad;
        if (sMaxX < rL || sMinX > rR || sMaxY < rT || sMinY > rB) return false;
        if (ptInRc(x1, y1, pad) || ptInRc(x2, y2, pad)) return true;
        if (distSeg(rL, rT, x1, y1, x2, y2) <= pad ||
            distSeg(rR, rT, x1, y1, x2, y2) <= pad ||
            distSeg(rR, rB, x1, y1, x2, y2) <= pad ||
            distSeg(rL, rB, x1, y1, x2, y2) <= pad ||
            distSeg((rL + rR) * 0.5f, (rT + rB) * 0.5f, x1, y1, x2, y2) <= pad) {
            return true;
        }
        auto ccw = [](float ax, float ay, float bx, float by, float cx, float cy) -> bool {
            return (cy - ay) * (bx - ax) > (by - ay) * (cx - ax);
        };
        auto segCross = [&](float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy) -> bool {
            return (ccw(ax, ay, cx, cy, dx, dy) != ccw(bx, by, cx, cy, dx, dy)) &&
                   (ccw(ax, ay, bx, by, cx, cy) != ccw(ax, ay, bx, by, dx, dy));
        };
        return segCross(x1, y1, x2, y2, rL, rT, rR, rT) ||
               segCross(x1, y1, x2, y2, rR, rT, rR, rB) ||
               segCross(x1, y1, x2, y2, rR, rB, rL, rB) ||
               segCross(x1, y1, x2, y2, rL, rB, rL, rT);
    };

    float pad = std::max(3.0f, a.strokeWidth * 0.5f);
    if ((a.tool == OverlayTool::Pen || a.tool == OverlayTool::Highlighter) && !a.points.empty()) {
        if (a.points.size() == 1) {
            return ptInRc(a.points[0].x, a.points[0].y, pad);
        }
        for (size_t i = 0; i + 1 < a.points.size(); ++i) {
            if (segIntersectsRc(a.points[i].x, a.points[i].y, a.points[i + 1].x, a.points[i + 1].y, pad)) {
                return true;
            }
        }
        return false;
    }
    if (a.tool == OverlayTool::Line || a.tool == OverlayTool::Arrow || a.tool == OverlayTool::NumberArrow) {
        if (a.tool == OverlayTool::NumberArrow) {
            float br = std::max(14.0f, a.strokeWidth * 2.0f + 8.0f);
            float cx = std::max(rL, std::min(rR, a.startPt.x));
            float cy = std::max(rT, std::min(rB, a.startPt.y));
            if (std::hypot(a.startPt.x - cx, a.startPt.y - cy) <= br) return true;
        }
        return segIntersectsRc(a.startPt.x, a.startPt.y, a.endPt.x, a.endPt.y, pad);
    }

    if (a.tool == OverlayTool::Rectangle) {
        float x0 = std::min(a.startPt.x, a.endPt.x);
        float x1 = std::max(a.startPt.x, a.endPt.x);
        float y0 = std::min(a.startPt.y, a.endPt.y);
        float y1 = std::max(a.startPt.y, a.endPt.y);
        return segIntersectsRc(x0, y0, x1, y0, pad) ||
               segIntersectsRc(x1, y0, x1, y1, pad) ||
               segIntersectsRc(x1, y1, x0, y1, pad) ||
               segIntersectsRc(x0, y1, x0, y0, pad);
    }

    float minX = std::min(a.startPt.x, a.endPt.x) - pad;
    float maxX = std::max(a.startPt.x, a.endPt.x) + pad;
    float minY = std::min(a.startPt.y, a.endPt.y) - pad;
    float maxY = std::max(a.startPt.y, a.endPt.y) + pad;
    return !(maxX < rL || minX > rR || maxY < rT || minY > rB);
}

int PepperSnapDaemon::HitTestAnnotation(float x, float y, DragMode* outHandleMode) const {
    if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;

    auto distToSeg = [](float px, float py, float x1, float y1, float x2, float y2) -> float {
        float dx = x2 - x1, dy = y2 - y1;
        float l2 = dx * dx + dy * dy;
        if (l2 < 1e-4f) return std::hypot(px - x1, py - y1);
        float t = std::max(0.0f, std::min(1.0f, ((px - x1) * dx + (py - y1) * dy) / l2));
        return std::hypot(px - (x1 + t * dx), py - (y1 + t * dy));
    };

    auto hitTriangle = [&](float px, float py, float ax, float ay, float bx, float by, float cx, float cy, float pad) -> bool {
        float d1 = (px - bx) * (ay - by) - (ax - bx) * (py - by);
        float d2 = (px - cx) * (by - cy) - (bx - cx) * (py - cy);
        float d3 = (px - ax) * (cy - ay) - (cx - ax) * (py - ay);
        bool hasNeg = (d1 < 0.0f) || (d2 < 0.0f) || (d3 < 0.0f);
        bool hasPos = (d1 > 0.0f) || (d2 > 0.0f) || (d3 > 0.0f);
        if (!(hasNeg && hasPos)) return true;
        return distToSeg(px, py, ax, ay, bx, by) <= pad ||
               distToSeg(px, py, bx, by, cx, cy) <= pad ||
               distToSeg(px, py, cx, cy, ax, ay) <= pad;
    };

    for (int i = (int)annotations.size() - 1; i >= 0; --i) {
        const auto& a = annotations[i];

        // Endpoint resize handles only when the annotation is already selected
        if (IsAnnotationSelected(a.id) &&
            a.tool != OverlayTool::Pen &&
            a.tool != OverlayTool::Highlighter &&
            a.tool != OverlayTool::TextBox) {
            float dxs = x - a.startPt.x, dys = y - a.startPt.y;
            if (dxs * dxs + dys * dys <= 49.0f) {
                if (outHandleMode) *outHandleMode = DragMode::DraggingAnnotationStart;
                return a.id;
            }
            float dxe = x - a.endPt.x, dye = y - a.endPt.y;
            if (dxe * dxe + dye * dye <= 49.0f) {
                if (outHandleMode) *outHandleMode = DragMode::DraggingAnnotationEnd;
                return a.id;
            }
        }

        // Direct geometry hit-testing per annotation tool (no bounding-box misselection)
        switch (a.tool) {
            case OverlayTool::Pen:
            case OverlayTool::Highlighter: {
                float halfW = (a.tool == OverlayTool::Highlighter)
                    ? std::max(5.0f, a.strokeWidth * 2.25f + 2.0f)
                    : std::max(4.5f, a.strokeWidth * 0.5f + 3.5f);
                if (a.points.size() == 1) {
                    if (std::hypot(x - a.points[0].x, y - a.points[0].y) <= halfW) {
                        if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                        return a.id;
                    }
                } else {
                    for (size_t j = 0; j + 1 < a.points.size(); ++j) {
                        if (distToSeg(x, y, a.points[j].x, a.points[j].y, a.points[j + 1].x, a.points[j + 1].y) <= halfW) {
                            if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                            return a.id;
                        }
                    }
                }
                break;
            }
            case OverlayTool::Line: {
                float halfW = std::max(4.5f, a.strokeWidth * 0.5f + 3.5f);
                if (distToSeg(x, y, a.startPt.x, a.startPt.y, a.endPt.x, a.endPt.y) <= halfW) {
                    if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                    return a.id;
                }
                break;
            }
            case OverlayTool::Arrow: {
                float halfW = std::max(4.5f, a.strokeWidth * 0.5f + 3.5f);
                if (distToSeg(x, y, a.startPt.x, a.startPt.y, a.endPt.x, a.endPt.y) <= halfW) {
                    if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                    return a.id;
                }
                float dx = a.endPt.x - a.startPt.x, dy = a.endPt.y - a.startPt.y;
                float len = std::hypot(dx, dy);
                if (len >= 2.0f) {
                    float angle = std::atan2(dy, dx);
                    float headLen = std::max(14.0f, a.strokeWidth * 3.8f);
                    float headAngle = 0.48f;
                    float ax = a.endPt.x, ay = a.endPt.y;
                    float bx = a.endPt.x - headLen * std::cos(angle - headAngle);
                    float by = a.endPt.y - headLen * std::sin(angle - headAngle);
                    float cx = a.endPt.x - headLen * std::cos(angle + headAngle);
                    float cy = a.endPt.y - headLen * std::sin(angle + headAngle);
                    if (hitTriangle(x, y, ax, ay, bx, by, cx, cy, 3.0f)) {
                        if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                        return a.id;
                    }
                }
                break;
            }
            case OverlayTool::NumberArrow: {
                float r = std::max(14.0f, a.strokeWidth * 2.0f + 8.0f);
                if (std::hypot(x - a.startPt.x, y - a.startPt.y) <= r + 2.5f) {
                    if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                    return a.id;
                }
                float halfW = std::max(4.5f, a.strokeWidth * 0.5f + 3.5f);
                if (distToSeg(x, y, a.startPt.x, a.startPt.y, a.endPt.x, a.endPt.y) <= halfW) {
                    if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                    return a.id;
                }
                float dx = a.endPt.x - a.startPt.x, dy = a.endPt.y - a.startPt.y;
                float len = std::hypot(dx, dy);
                if (len > r + 4.0f) {
                    float angle = std::atan2(dy, dx);
                    float headLen = std::max(14.0f, a.strokeWidth * 3.8f);
                    float headAngle = 0.48f;
                    float ax = a.endPt.x, ay = a.endPt.y;
                    float bx = a.endPt.x - headLen * std::cos(angle - headAngle);
                    float by = a.endPt.y - headLen * std::sin(angle - headAngle);
                    float cx = a.endPt.x - headLen * std::cos(angle + headAngle);
                    float cy = a.endPt.y - headLen * std::sin(angle + headAngle);
                    if (hitTriangle(x, y, ax, ay, bx, by, cx, cy, 3.0f)) {
                        if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                        return a.id;
                    }
                }
                break;
            }
            case OverlayTool::Rectangle: {
                float x0 = std::min(a.startPt.x, a.endPt.x);
                float x1 = std::max(a.startPt.x, a.endPt.x);
                float y0 = std::min(a.startPt.y, a.endPt.y);
                float y1 = std::max(a.startPt.y, a.endPt.y);
                float halfW = std::max(4.5f, a.strokeWidth * 0.5f + 3.5f);
                if (distToSeg(x, y, x0, y0, x1, y0) <= halfW ||
                    distToSeg(x, y, x1, y0, x1, y1) <= halfW ||
                    distToSeg(x, y, x1, y1, x0, y1) <= halfW ||
                    distToSeg(x, y, x0, y1, x0, y0) <= halfW) {
                    if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                    return a.id;
                }
                break;
            }
            case OverlayTool::Ellipse: {
                float x0 = std::min(a.startPt.x, a.endPt.x);
                float x1 = std::max(a.startPt.x, a.endPt.x);
                float y0 = std::min(a.startPt.y, a.endPt.y);
                float y1 = std::max(a.startPt.y, a.endPt.y);
                float rx = (x1 - x0) * 0.5f;
                float ry = (y1 - y0) * 0.5f;
                float halfW = std::max(4.5f, a.strokeWidth * 0.5f + 3.5f);
                if (rx < 2.0f || ry < 2.0f) {
                    if (distToSeg(x, y, a.startPt.x, a.startPt.y, a.endPt.x, a.endPt.y) <= halfW) {
                        if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                        return a.id;
                    }
                } else {
                    float cx = (x0 + x1) * 0.5f;
                    float cy = (y0 + y1) * 0.5f;
                    float dx = x - cx;
                    float dy = y - cy;
                    float normLen = std::hypot(dx / rx, dy / ry);
                    if (normLen > 1e-4f) {
                        float radialDist = std::hypot(dx, dy) * std::abs(1.0f - 1.0f / normLen);
                        if (radialDist <= halfW) {
                            if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                            return a.id;
                        }
                    }
                }
                break;
            }
            case OverlayTool::TextBox: {
                float minX = std::min(a.startPt.x, a.endPt.x) - 2.0f;
                float maxX = std::max(a.startPt.x, a.endPt.x) + 2.0f;
                float minY = std::min(a.startPt.y, a.endPt.y) - 2.0f;
                float maxY = std::max(a.startPt.y, a.endPt.y) + 2.0f;
                if (x >= minX && x <= maxX && y >= minY && y <= maxY) {
                    if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                    return a.id;
                }
                break;
            }
            case OverlayTool::MosaicSquare: {
                float minX = std::min(a.startPt.x, a.endPt.x) - 3.0f;
                float maxX = std::max(a.startPt.x, a.endPt.x) + 3.0f;
                float minY = std::min(a.startPt.y, a.endPt.y) - 3.0f;
                float maxY = std::max(a.startPt.y, a.endPt.y) + 3.0f;
                if (x >= minX && x <= maxX && y >= minY && y <= maxY) {
                    if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                    return a.id;
                }
                break;
            }
            case OverlayTool::MosaicCircle: {
                float x0 = std::min(a.startPt.x, a.endPt.x);
                float x1 = std::max(a.startPt.x, a.endPt.x);
                float y0 = std::min(a.startPt.y, a.endPt.y);
                float y1 = std::max(a.startPt.y, a.endPt.y);
                float rx = std::max(2.0f, (x1 - x0) * 0.5f + 3.0f);
                float ry = std::max(2.0f, (y1 - y0) * 0.5f + 3.0f);
                float cx = (x0 + x1) * 0.5f;
                float cy = (y0 + y1) * 0.5f;
                float nx = (x - cx) / rx;
                float ny = (y - cy) / ry;
                if (nx * nx + ny * ny <= 1.0f) {
                    if (outHandleMode) *outHandleMode = DragMode::MovingAnnotation;
                    return a.id;
                }
                break;
            }
            default:
                break;
        }
    }
    return -1;
}

void GetPinnedScaledDims(const PinnedWindowData* data, int& outW, int& outH);

Bitmap* PepperSnapDaemon::RenderCroppedRegionBitmap() {
    CommitActiveTextBox();
    CommitActiveSizeInput();
    CommitActiveRatioInput();
    CommitActiveStrokeInput();
    if (!frozenDesktopBmp || !hasSelection) return nullptr;
    int rx = std::max(0, (int)std::min(selRect.left, selRect.right));
    int ry = std::max(0, (int)std::min(selRect.top, selRect.bottom));
    int rw = std::min(vScreenW - rx, (int)std::abs(selRect.right - selRect.left));
    int rh = std::min(vScreenH - ry, (int)std::abs(selRect.bottom - selRect.top));
    if (rw < 1 || rh < 1) return nullptr;

    Bitmap* out = nullptr;
    Bitmap* activeImageSource = (!overlayFrames.empty() && overlayActiveFrameIdx < overlayFrames.size())
        ? overlayFrames[overlayActiveFrameIdx]
        : overlaySingleImageBmp;
    if (activeImageSource && overlayImgDrawRect.right > overlayImgDrawRect.left && overlayImgDrawRect.bottom > overlayImgDrawRect.top) {
        out = new Bitmap(rw, rh, PixelFormat32bppARGB);
        if (out) {
            Graphics gOut(out);
            gOut.SetCompositingMode(CompositingModeSourceCopy);
            gOut.Clear(Color(0, 0, 0, 0));
            gOut.SetCompositingMode(CompositingModeSourceOver);
            gOut.SetInterpolationMode(InterpolationModeHighQualityBicubic);
            gOut.SetPixelOffsetMode(PixelOffsetModeHalf);
            int drawX = overlayImgDrawRect.left - rx;
            int drawY = overlayImgDrawRect.top - ry;
            int drawW = overlayImgDrawRect.right - overlayImgDrawRect.left;
            int drawH = overlayImgDrawRect.bottom - overlayImgDrawRect.top;
            ImageAttributes ia;
            ia.SetWrapMode(WrapModeTileFlipXY);
            gOut.DrawImage(activeImageSource, Rect(drawX, drawY, drawW, drawH),
                           0, 0, (INT)activeImageSource->GetWidth(), (INT)activeImageSource->GetHeight(), UnitPixel, &ia);

            if (pinnedImageAboveEdit) {
                RECT selOverlayRc = { rx, ry, rx + rw, ry + rh };
                std::vector<HWND> zOrderedPins;
                HWND hPinCur = nullptr;
                while ((hPinCur = FindWindowExW(nullptr, hPinCur, L"PepperSnapPinWnd", nullptr)) != nullptr) {
                    if (IsWindow(hPinCur) && IsWindowVisible(hPinCur) &&
                        std::find(pinnedWindows.begin(), pinnedWindows.end(), hPinCur) != pinnedWindows.end()) {
                        zOrderedPins.push_back(hPinCur);
                    }
                }
                for (HWND hp : pinnedWindows) {
                    if (hp && IsWindow(hp) && IsWindowVisible(hp) &&
                        std::find(zOrderedPins.begin(), zOrderedPins.end(), hp) == zOrderedPins.end()) {
                        zOrderedPins.push_back(hp);
                    }
                }
                for (auto it = zOrderedPins.rbegin(); it != zOrderedPins.rend(); ++it) {
                    HWND hp = *it;
                    PinnedWindowData* pData = (PinnedWindowData*)GetWindowLongPtrW(hp, GWLP_USERDATA);
                    if (!pData || !pData->bmp) continue;
                    int pinW = 32, pinH = 32;
                    GetPinnedScaledDims(pData, pinW, pinH);
                    int pinOverlayX = pData->screenImgX - vScreenX;
                    int pinOverlayY = pData->screenImgY - vScreenY;
                    RECT pinOverlayRc = { pinOverlayX, pinOverlayY, pinOverlayX + pinW, pinOverlayY + pinH };
                    RECT interRc = {0};
                    if (!IntersectRect(&interRc, &selOverlayRc, &pinOverlayRc)) continue;

                    int srcW = std::max(1, (int)pData->bmp->GetWidth());
                    int srcH = std::max(1, (int)pData->bmp->GetHeight());
                    double fitScale = std::min((double)pinW / (double)srcW, (double)pinH / (double)srcH);
                    int fitW = std::max(1, std::min(pinW, (int)std::round(srcW * fitScale)));
                    int fitH = std::max(1, std::min(pinH, (int)std::round(srcH * fitScale)));
                    int fitX = (pinOverlayX - rx) + (pinW - fitW) / 2;
                    int fitY = (pinOverlayY - ry) + (pinH - fitH) / 2;

                    gOut.SetInterpolationMode(pData->smoothImage ? InterpolationModeHighQualityBicubic : InterpolationModeNearestNeighbor);
                    gOut.SetPixelOffsetMode(PixelOffsetModeHalf);

                    double clipX0 = std::max(0.0, (double)fitX);
                    double clipY0 = std::max(0.0, (double)fitY);
                    double clipX1 = std::min((double)rw, (double)(fitX + fitW));
                    double clipY1 = std::min((double)rh, (double)(fitY + fitH));
                    if (clipX1 > clipX0 && clipY1 > clipY0) {
                        double srcX0 = (clipX0 - (double)fitX) * (double)srcW / (double)fitW;
                        double srcY0 = (clipY0 - (double)fitY) * (double)srcH / (double)fitH;
                        double srcX1 = (clipX1 - (double)fitX) * (double)srcW / (double)fitW;
                        double srcY1 = (clipY1 - (double)fitY) * (double)srcH / (double)fitH;
                        int sLeft   = std::max(0, (int)std::floor(srcX0) - 2);
                        int sTop    = std::max(0, (int)std::floor(srcY0) - 2);
                        int sRight  = std::min(srcW, (int)std::ceil(srcX1) + 2);
                        int sBottom = std::min(srcH, (int)std::ceil(srcY1) + 2);
                        int subSrcW = sRight - sLeft;
                        int subSrcH = sBottom - sTop;
                        if (subSrcW > 0 && subSrcH > 0) {
                            REAL subDstX = (REAL)((double)fitX + (double)sLeft * (double)fitW / (double)srcW);
                            REAL subDstY = (REAL)((double)fitY + (double)sTop  * (double)fitH / (double)srcH);
                            REAL subDstW = (REAL)((double)subSrcW * (double)fitW / (double)srcW);
                            REAL subDstH = (REAL)((double)subSrcH * (double)fitH / (double)srcH);
                            gOut.DrawImage(pData->bmp, RectF(subDstX, subDstY, subDstW, subDstH),
                                           (REAL)sLeft, (REAL)sTop, (REAL)subSrcW, (REAL)subSrcH, UnitPixel, &ia);
                        }
                    }
                }
            }
        }
    }
    if (!out) {
        out = frozenDesktopBmp->Clone(rx, ry, rw, rh, PixelFormat32bppARGB);
    }
    if (!out) return nullptr;

    // Pass 1: Real-time pixelation filters (Mosaic Square & Mosaic Circle) applied directly to cropped bitmap
    for (const auto& ann : annotations) {
        if (ann.tool == OverlayTool::MosaicSquare || ann.tool == OverlayTool::MosaicCircle) {
            int px = (int)std::min(ann.startPt.x, ann.endPt.x) - rx;
            int py = (int)std::min(ann.startPt.y, ann.endPt.y) - ry;
            int pw = (int)std::abs(ann.endPt.x - ann.startPt.x);
            int ph = (int)std::abs(ann.endPt.y - ann.startPt.y);
            int block = GetMosaicBlockSize(ann.strokeWidth);
            ApplyPixelateFilter(out, px, py, pw, ph, block, ann.tool == OverlayTool::MosaicCircle);
        }
    }

    // Pass 2: Highlighter layer (supports Non-Stacking or Stacking mode) + Vector & Text annotations
    Graphics g(out);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    RenderHighlighterLayer(g, rx, ry, rw, rh, annotations, nullptr, nonStackingHighlighter);
    for (const auto& ann : annotations) {
        if (ann.tool != OverlayTool::MosaicSquare &&
            ann.tool != OverlayTool::MosaicCircle &&
            ann.tool != OverlayTool::Highlighter) {
            DrawVectorAnnotation(g, ann, rx, ry, false, false);
        }
    }
    if (hasEllipseSelection) {
        ApplyEllipseMaskToBitmap(out);
    } else if (hasCustomQuad) {
        ApplyQuadPolygonMaskToBitmap(out, selQuad, rx, ry);
    }
    return out;
}

// ----------------------------------------------------------------------------
// Vector Tool Icons, Hover Speech-Bubble Tooltip & Docked HUD Builder
// ----------------------------------------------------------------------------

void PepperSnapDaemon::DrawDockButtonIcon(Graphics& g, const DockButton& b, const RectF& rf) {
    float cx = rf.X + rf.Width * 0.5f;
    float cy = rf.Y + rf.Height * 0.5f;
    Pen whitePen(Color(255, 248, 250, 252), 1.8f);
    whitePen.SetStartCap(LineCapRound);
    whitePen.SetEndCap(LineCapRound);
    whitePen.SetLineJoin(LineJoinRound);
    SolidBrush whiteBrush(Color(255, 248, 250, 252));

    if (b.isTool) {
        switch (b.tool) {
            case OverlayTool::SelectMove: {
                // Cursor arrow icon
                PointF pts[7] = {
                    PointF(cx - 5.0f, cy - 8.0f),
                    PointF(cx - 5.0f, cy + 6.0f),
                    PointF(cx - 1.5f, cy + 3.0f),
                    PointF(cx + 1.5f, cy + 8.5f),
                    PointF(cx + 3.8f, cy + 7.2f),
                    PointF(cx + 0.8f, cy + 1.8f),
                    PointF(cx + 5.5f, cy + 1.8f)
                };
                g.FillPolygon(&whiteBrush, pts, 7);
                break;
            }
            case OverlayTool::Pen: {
                // Pen icon angled down-left
                PointF body[4] = {
                    PointF(cx - 3.0f, cy + 1.0f),
                    PointF(cx + 4.5f, cy - 6.5f),
                    PointF(cx + 7.5f, cy - 3.5f),
                    PointF(cx + 0.0f, cy + 4.0f)
                };
                g.DrawPolygon(&whitePen, body, 4);
                PointF tip[3] = {
                    PointF(cx - 7.0f, cy + 7.5f),
                    PointF(cx - 3.0f, cy + 1.0f),
                    PointF(cx + 0.0f, cy + 4.0f)
                };
                g.FillPolygon(&whiteBrush, tip, 3);
                break;
            }
            case OverlayTool::Highlighter: {
                // Stabilo chisel-tip marker icon
                PointF body[4] = {
                    PointF(cx - 2.5f, cy + 0.5f),
                    PointF(cx + 3.5f, cy - 5.5f),
                    PointF(cx + 7.5f, cy - 1.5f),
                    PointF(cx + 1.5f, cy + 4.5f)
                };
                SolidBrush hiFill(Color(255, 250, 204, 21));
                g.FillPolygon(&hiFill, body, 4);
                g.DrawPolygon(&whitePen, body, 4);
                PointF chisel[4] = {
                    PointF(cx - 7.0f, cy + 6.5f),
                    PointF(cx - 3.5f, cy + 1.5f),
                    PointF(cx + 0.5f, cy + 5.5f),
                    PointF(cx - 3.5f, cy + 7.5f)
                };
                g.FillPolygon(&whiteBrush, chisel, 4);
                break;
            }
            case OverlayTool::Line: {
                g.DrawLine(&whitePen, cx - 6.5f, cy + 6.0f, cx + 6.5f, cy - 6.0f);
                g.FillEllipse(&whiteBrush, cx - 8.0f, cy + 4.5f, 3.5f, 3.5f);
                g.FillEllipse(&whiteBrush, cx + 4.5f, cy - 8.0f, 3.5f, 3.5f);
                break;
            }
            case OverlayTool::Arrow: {
                g.DrawLine(&whitePen, cx - 6.5f, cy + 6.0f, cx + 4.5f, cy - 5.0f);
                PointF head[3] = {
                    PointF(cx + 7.0f, cy - 7.0f),
                    PointF(cx + 0.5f, cy - 6.5f),
                    PointF(cx + 6.0f, cy - 0.5f)
                };
                g.FillPolygon(&whiteBrush, head, 3);
                break;
            }
            case OverlayTool::NumberArrow: {
                // Circle badge with '1' + small arrow
                g.DrawEllipse(&whitePen, cx - 8.5f, cy - 3.5f, 9.5f, 9.5f);
                FontFamily ff(L"Segoe UI");
                Font f(&ff, 8.5f, FontStyleBold, UnitPixel);
                g.DrawString(L"1", -1, &f, PointF(cx - 6.8f, cy - 4.2f), &whiteBrush);
                g.DrawLine(&whitePen, cx + 1.0f, cy - 0.5f, cx + 7.5f, cy - 6.0f);
                PointF head[3] = {
                    PointF(cx + 8.5f, cy - 7.0f),
                    PointF(cx + 3.5f, cy - 7.0f),
                    PointF(cx + 8.0f, cy - 2.0f)
                };
                g.FillPolygon(&whiteBrush, head, 3);
                break;
            }
            case OverlayTool::Rectangle: {
                g.DrawRectangle(&whitePen, cx - 7.0f, cy - 5.5f, 14.0f, 11.0f);
                break;
            }
            case OverlayTool::Ellipse: {
                g.DrawEllipse(&whitePen, cx - 7.5f, cy - 6.0f, 15.0f, 12.0f);
                break;
            }
            case OverlayTool::TextBox: {
                FontFamily ff(L"Georgia");
                Font f(&ff, 14.0f, FontStyleBold, UnitPixel);
                StringFormat sf;
                sf.SetAlignment(StringAlignmentCenter);
                sf.SetLineAlignment(StringAlignmentCenter);
                g.DrawString(L"T", -1, &f, rf, &sf, &whiteBrush);
                break;
            }
            case OverlayTool::MosaicSquare: {
                Pen thinPen(Color(230, 248, 250, 252), 1.2f);
                g.DrawRectangle(&thinPen, cx - 7.0f, cy - 6.0f, 14.0f, 12.0f);
                for (int r = 0; r < 3; ++r) {
                    for (int c = 0; c < 3; ++c) {
                        if ((r + c) % 2 == 0) {
                            g.FillRectangle(&whiteBrush, cx - 6.0f + c * 4.0f, cy - 5.0f + r * 3.5f, 4.0f, 3.5f);
                        }
                    }
                }
                break;
            }
            case OverlayTool::MosaicCircle: {
                Pen thinPen(Color(230, 248, 250, 252), 1.4f);
                g.DrawEllipse(&thinPen, cx - 7.0f, cy - 7.0f, 14.0f, 14.0f);
                g.FillRectangle(&whiteBrush, cx - 4.0f, cy - 4.0f, 3.8f, 3.8f);
                g.FillRectangle(&whiteBrush, cx + 0.2f, cy + 0.2f, 3.8f, 3.8f);
                SolidBrush dimB(Color(140, 248, 250, 252));
                g.FillRectangle(&dimB, cx + 0.2f, cy - 4.0f, 3.8f, 3.8f);
                g.FillRectangle(&dimB, cx - 4.0f, cy + 0.2f, 3.8f, 3.8f);
                break;
            }
        }
        return;
    }

    // Action Button Icons
    switch (b.id) {
        case DBTN_ACT_UNDO: {
            g.DrawArc(&whitePen, cx - 6.0f, cy - 4.0f, 12.0f, 10.0f, 180.0f, 180.0f);
            PointF head[3] = { PointF(cx - 7.5f, cy - 1.0f), PointF(cx - 3.5f, cy - 5.0f), PointF(cx - 2.5f, cy + 0.5f) };
            g.FillPolygon(&whiteBrush, head, 3);
            break;
        }
        case DBTN_ACT_REDO: {
            g.DrawArc(&whitePen, cx - 6.0f, cy - 4.0f, 12.0f, 10.0f, 180.0f, 180.0f);
            PointF head[3] = { PointF(cx + 7.5f, cy - 1.0f), PointF(cx + 3.5f, cy - 5.0f), PointF(cx + 2.5f, cy + 0.5f) };
            g.FillPolygon(&whiteBrush, head, 3);
            break;
        }
        case DBTN_ACT_CLEAR: {
            // Trash bin icon
            g.DrawRectangle(&whitePen, cx - 5.0f, cy - 3.0f, 10.0f, 10.0f);
            g.DrawLine(&whitePen, cx - 7.0f, cy - 3.5f, cx + 7.0f, cy - 3.5f);
            g.DrawLine(&whitePen, cx - 2.5f, cy - 6.0f, cx + 2.5f, cy - 6.0f);
            break;
        }
        case DBTN_ACT_RESET_NUM: {
            // Circular reset arrow + '1' icon below Numbering Arrow
            Pen amberPen(Color(255, 250, 204, 21), 1.7f);
            SolidBrush amberBrush(Color(255, 250, 204, 21));
            g.DrawArc(&amberPen, cx - 8.5f, cy - 5.5f, 10.5f, 10.5f, 35.0f, 275.0f);
            PointF head[3] = {
                PointF(cx - 8.5f, cy - 4.0f),
                PointF(cx - 4.5f, cy - 6.8f),
                PointF(cx - 4.5f, cy - 1.8f)
            };
            g.FillPolygon(&amberBrush, head, 3);
            FontFamily ff(L"Segoe UI");
            Font f(&ff, 10.5f, FontStyleBold, UnitPixel);
            g.DrawString(L"1", -1, &f, PointF(cx + 1.5f, cy - 6.5f), &amberBrush);
            break;
        }
        case DBTN_ACT_PIN: {
            // Pushpin icon
            g.FillEllipse(&whiteBrush, cx - 4.0f, cy - 6.5f, 8.0f, 5.0f);
            g.DrawLine(&whitePen, cx, cy - 2.0f, cx, cy + 7.0f);
            g.DrawLine(&whitePen, cx - 5.0f, cy + 1.0f, cx + 5.0f, cy + 1.0f);
            break;
        }
        case DBTN_ACT_OCR: {
            // Magnifying glass with 'T' inside the glass circle
            float gcx = cx - 1.8f;
            float gcy = cy - 1.8f;
            float gr  = 6.2f;
            g.DrawEllipse(&whitePen, gcx - gr, gcy - gr, gr * 2.0f, gr * 2.0f);

            Pen handlePen(Color(255, 248, 250, 252), 2.2f);
            handlePen.SetStartCap(LineCapRound);
            handlePen.SetEndCap(LineCapRound);
            g.DrawLine(&handlePen, gcx + 4.4f, gcy + 4.4f, cx + 7.2f, cy + 7.2f);

            Pen tPen(Color(255, 248, 250, 252), 1.6f);
            tPen.SetStartCap(LineCapRound);
            tPen.SetEndCap(LineCapRound);
            g.DrawLine(&tPen, gcx - 2.7f, gcy - 2.5f, gcx + 2.7f, gcy - 2.5f);
            g.DrawLine(&tPen, gcx,        gcy - 2.5f, gcx,        gcy + 3.1f);
            break;
        }
        case DBTN_ACT_SEARCH_GOOGLE:
        case DBTN_ACT_PIN_SEARCH_GOOGLE: {
            // Internet globe: outer ring, equator, one meridian ellipse and two
            // latitude chords. A magnifier with a globe in the lens was tried
            // first and read as "search inside the image" (OCR and zoom already
            // own that glyph family), so the lens and handle are gone.
            const float gr = 7.4f;
            g.DrawEllipse(&whitePen, cx - gr, cy - gr, gr * 2.0f, gr * 2.0f);

            Pen meridianPen(Color(255, 248, 250, 252), 1.3f);
            g.DrawLine(&meridianPen, cx - gr, cy, cx + gr, cy);
            g.DrawEllipse(&meridianPen, cx - gr * 0.46f, cy - gr, gr * 0.92f, gr * 2.0f);

            // Latitude chords sit at +/-0.53r, where the circle is 0.85r wide.
            Pen latPen(Color(255, 248, 250, 252), 1.1f);
            latPen.SetStartCap(LineCapRound);
            latPen.SetEndCap(LineCapRound);
            g.DrawLine(&latPen, cx - gr * 0.85f, cy - gr * 0.53f, cx + gr * 0.85f, cy - gr * 0.53f);
            g.DrawLine(&latPen, cx - gr * 0.85f, cy + gr * 0.53f, cx + gr * 0.85f, cy + gr * 0.53f);
            break;
        }
        case DBTN_ACT_ZOOM_OUT: {
            // Magnifying glass with '-' inside the glass circle
            float gcx = cx - 1.8f;
            float gcy = cy - 1.8f;
            float gr  = 6.2f;
            g.DrawEllipse(&whitePen, gcx - gr, gcy - gr, gr * 2.0f, gr * 2.0f);

            Pen handlePen(Color(255, 248, 250, 252), 2.2f);
            handlePen.SetStartCap(LineCapRound);
            handlePen.SetEndCap(LineCapRound);
            g.DrawLine(&handlePen, gcx + 4.4f, gcy + 4.4f, cx + 7.2f, cy + 7.2f);

            Pen symPen(Color(255, 248, 250, 252), 1.7f);
            symPen.SetStartCap(LineCapRound);
            symPen.SetEndCap(LineCapRound);
            g.DrawLine(&symPen, gcx - 2.8f, gcy, gcx + 2.8f, gcy);
            break;
        }
        case DBTN_ACT_ZOOM_IN: {
            // Magnifying glass with '+' inside the glass circle
            float gcx = cx - 1.8f;
            float gcy = cy - 1.8f;
            float gr  = 6.2f;
            g.DrawEllipse(&whitePen, gcx - gr, gcy - gr, gr * 2.0f, gr * 2.0f);

            Pen handlePen(Color(255, 248, 250, 252), 2.2f);
            handlePen.SetStartCap(LineCapRound);
            handlePen.SetEndCap(LineCapRound);
            g.DrawLine(&handlePen, gcx + 4.4f, gcy + 4.4f, cx + 7.2f, cy + 7.2f);

            Pen symPen(Color(255, 248, 250, 252), 1.7f);
            symPen.SetStartCap(LineCapRound);
            symPen.SetEndCap(LineCapRound);
            g.DrawLine(&symPen, gcx - 2.8f, gcy,        gcx + 2.8f, gcy);
            g.DrawLine(&symPen, gcx,        gcy - 2.8f, gcx,        gcy + 2.8f);
            break;
        }
        case DBTN_ACT_ZOOM_RESET: {
            // Magnifying glass with '=' inside the glass circle (Reset to original size)
            float gcx = cx - 1.8f;
            float gcy = cy - 1.8f;
            float gr  = 6.2f;
            g.DrawEllipse(&whitePen, gcx - gr, gcy - gr, gr * 2.0f, gr * 2.0f);

            Pen handlePen(Color(255, 248, 250, 252), 2.2f);
            handlePen.SetStartCap(LineCapRound);
            handlePen.SetEndCap(LineCapRound);
            g.DrawLine(&handlePen, gcx + 4.4f, gcy + 4.4f, cx + 7.2f, cy + 7.2f);

            Pen symPen(Color(255, 248, 250, 252), 1.6f);
            symPen.SetStartCap(LineCapRound);
            symPen.SetEndCap(LineCapRound);
            g.DrawLine(&symPen, gcx - 2.6f, gcy - 1.6f, gcx + 2.6f, gcy - 1.6f);
            g.DrawLine(&symPen, gcx - 2.6f, gcy + 1.6f, gcx + 2.6f, gcy + 1.6f);
            break;
        }
        case DBTN_ACT_POS_RESET: {
            // 4-way position cross icon (Reset to original position & size)
            Pen posPen(Color(255, 248, 250, 252), 1.55f);
            SolidBrush posBr(Color(255, 248, 250, 252));
            g.DrawLine(&posPen, cx - 6.2f, cy, cx + 6.2f, cy);
            g.DrawLine(&posPen, cx, cy - 6.2f, cx, cy + 6.2f);
            PointF upH[3]    = { PointF(cx, cy - 8.2f), PointF(cx - 2.5f, cy - 5.0f), PointF(cx + 2.5f, cy - 5.0f) };
            PointF downH[3]  = { PointF(cx, cy + 8.2f), PointF(cx - 2.5f, cy + 5.0f), PointF(cx + 2.5f, cy + 5.0f) };
            PointF leftH[3]  = { PointF(cx - 8.2f, cy), PointF(cx - 5.0f, cy - 2.5f), PointF(cx - 5.0f, cy + 2.5f) };
            PointF rightH[3] = { PointF(cx + 8.2f, cy), PointF(cx + 5.0f, cy - 2.5f), PointF(cx + 5.0f, cy + 2.5f) };
            g.FillPolygon(&posBr, upH, 3);
            g.FillPolygon(&posBr, downH, 3);
            g.FillPolygon(&posBr, leftH, 3);
            g.FillPolygon(&posBr, rightH, 3);
            break;
        }
        case DBTN_ACT_PIN_OUTLINE: {
            // Dotted-line outline icon (between Zoom Reset and Hide Toolbar)
            Pen dotPen(Color(255, 248, 250, 252), 1.7f);
            REAL dashVals[2] = { 1.6f, 1.8f };
            dotPen.SetDashPattern(dashVals, 2);
            dotPen.SetLineJoin(LineJoinMiter);
            g.DrawRectangle(&dotPen, cx - 6.5f, cy - 5.5f, 13.0f, 11.0f);
            break;
        }
        case DBTN_ACT_PIN_HIDE_IMAGE:
        case DBTN_ACT_PIN_HIDE_TOOLBAR: {
            // Typical eye icon (Hide / Show)
            Pen eyePen(Color(255, 248, 250, 252), 1.65f);
            eyePen.SetStartCap(LineCapRound);
            eyePen.SetEndCap(LineCapRound);
            eyePen.SetLineJoin(LineJoinRound);
            GraphicsPath eyePath;
            PointF topPts[3] = {
                PointF(cx - 7.4f, cy),
                PointF(cx,        cy - 4.9f),
                PointF(cx + 7.4f, cy)
            };
            PointF botPts[3] = {
                PointF(cx + 7.4f, cy),
                PointF(cx,        cy + 4.9f),
                PointF(cx - 7.4f, cy)
            };
            eyePath.StartFigure();
            eyePath.AddCurve(topPts, 3, 0.55f);
            eyePath.AddCurve(botPts, 3, 0.55f);
            eyePath.CloseFigure();
            g.DrawPath(&eyePen, &eyePath);
            g.DrawEllipse(&eyePen, cx - 2.2f, cy - 2.2f, 4.4f, 4.4f);
            break;
        }
        case DBTN_ACT_PIN_UNFILTER: {
            // Water-drop blur / smooth icon (between Hide Outline and Save As)
            Pen dropPen(Color(255, 248, 250, 252), 1.75f);
            dropPen.SetLineJoin(LineJoinRound);
            dropPen.SetStartCap(LineCapRound);
            dropPen.SetEndCap(LineCapRound);

            float bulbCy = cy + 1.8f;
            float r      = 4.6f;
            GraphicsPath dropPath;
            dropPath.StartFigure();
            dropPath.AddLine(PointF(cx, cy - 6.8f), PointF(cx + 3.98f, bulbCy - 2.3f));
            dropPath.AddArc(cx - r, bulbCy - r, r * 2.0f, r * 2.0f, -30.0f, 240.0f);
            dropPath.CloseFigure();
            g.DrawPath(&dropPen, &dropPath);

            Pen shinePen(Color(220, 248, 250, 252), 1.3f);
            shinePen.SetStartCap(LineCapRound);
            shinePen.SetEndCap(LineCapRound);
            g.DrawArc(&shinePen, cx - 2.4f, bulbCy - 2.4f, 4.8f, 4.8f, 20.0f, 80.0f);
            break;
        }
        case DBTN_ACT_SAVE_AS: {
            // Floppy + ellipsis
            g.DrawRectangle(&whitePen, cx - 6.5f, cy - 6.5f, 11.0f, 12.0f);
            g.FillRectangle(&whiteBrush, cx - 3.5f, cy - 6.5f, 5.0f, 4.0f);
            g.FillEllipse(&whiteBrush, cx + 6.0f, cy + 3.5f, 2.5f, 2.5f);
            break;
        }
        case DBTN_ACT_SAVE: {
            // Floppy disk icon
            g.DrawRectangle(&whitePen, cx - 6.0f, cy - 6.0f, 12.0f, 12.0f);
            g.FillRectangle(&whiteBrush, cx - 3.5f, cy - 6.0f, 6.0f, 4.5f);
            g.DrawRectangle(&whitePen, cx - 3.5f, cy + 1.0f, 7.0f, 5.0f);
            break;
        }
        case DBTN_ACT_COPY: {
            if (b.label.empty()) {
                // Centered copy sheets icon (1-square button)
                g.DrawRectangle(&whitePen, cx - 5.5f, cy - 6.5f, 8.0f, 10.0f);
                g.DrawRectangle(&whitePen, cx - 2.5f, cy - 3.5f, 8.0f, 10.0f);
            } else {
                // Copy sheets + label
                g.DrawRectangle(&whitePen, cx - 18.0f, cy - 6.0f, 8.0f, 10.0f);
                g.DrawRectangle(&whitePen, cx - 15.0f, cy - 3.0f, 8.0f, 10.0f);
                FontFamily ff(L"Segoe UI");
                Font f(&ff, 11.0f, FontStyleBold, UnitPixel);
                g.DrawString(b.label.c_str(), -1, &f, PointF(cx - 4.0f, cy - 7.0f), &whiteBrush);
            }
            break;
        }
        case DBTN_ACT_CLOSE: {
            g.DrawLine(&whitePen, cx - 4.5f, cy - 4.5f, cx + 4.5f, cy + 4.5f);
            g.DrawLine(&whitePen, cx + 4.5f, cy - 4.5f, cx - 4.5f, cy + 4.5f);
            break;
        }
        case DBTN_ACT_DRAG_HUD: {
            // 4-way move cross icon for dragging the combined 3-row toolbar
            Pen movePen(Color(255, 203, 213, 225), 1.5f);
            SolidBrush moveBr(Color(255, 203, 213, 225));
            g.DrawLine(&movePen, cx - 6.5f, cy, cx + 6.5f, cy);
            g.DrawLine(&movePen, cx, cy - 6.5f, cx, cy + 6.5f);
            PointF upH[3]    = { PointF(cx, cy - 8.5f), PointF(cx - 2.6f, cy - 5.2f), PointF(cx + 2.6f, cy - 5.2f) };
            PointF downH[3]  = { PointF(cx, cy + 8.5f), PointF(cx - 2.6f, cy + 5.2f), PointF(cx + 2.6f, cy + 5.2f) };
            PointF leftH[3]  = { PointF(cx - 8.5f, cy), PointF(cx - 5.2f, cy - 2.6f), PointF(cx - 5.2f, cy + 2.6f) };
            PointF rightH[3] = { PointF(cx + 8.5f, cy), PointF(cx + 5.2f, cy - 2.6f), PointF(cx + 5.2f, cy + 2.6f) };
            g.FillPolygon(&moveBr, upH, 3);
            g.FillPolygon(&moveBr, downH, 3);
            g.FillPolygon(&moveBr, leftH, 3);
            g.FillPolygon(&moveBr, rightH, 3);
            break;
        }
        case DBTN_ACT_FRAME_PLAY_PAUSE: {
            // Play triangle on left + Pause bars on right
            PointF tri[3] = {
                PointF(cx - 6.8f, cy - 5.5f),
                PointF(cx - 6.8f, cy + 5.5f),
                PointF(cx + 0.2f, cy)
            };
            g.FillPolygon(&whiteBrush, tri, 3);
            g.FillRectangle(&whiteBrush, cx + 2.0f, cy - 5.5f, 2.2f, 11.0f);
            g.FillRectangle(&whiteBrush, cx + 5.4f, cy - 5.5f, 2.2f, 11.0f);
            break;
        }
        case DBTN_ACT_FRAME_TOGGLE: {
            // Frame strip cards + dropdown chevron below
            Pen thinFramePen(Color(255, 248, 250, 252), 1.35f);
            g.DrawRectangle(&whitePen, cx - 7.0f, cy - 6.2f, 14.0f, 7.6f);
            g.DrawLine(&thinFramePen, cx - 2.3f, cy - 6.2f, cx - 2.3f, cy + 1.4f);
            g.DrawLine(&thinFramePen, cx + 2.3f, cy - 6.2f, cx + 2.3f, cy + 1.4f);
            Pen chevPen(Color(255, 248, 250, 252), 1.85f);
            chevPen.SetStartCap(LineCapRound);
            chevPen.SetEndCap(LineCapRound);
            g.DrawLine(&chevPen, cx - 4.0f, cy + 3.8f, cx, cy + 7.2f);
            g.DrawLine(&chevPen, cx, cy + 7.2f, cx + 4.0f, cy + 3.8f);
            break;
        }
        case DBTN_ACT_SCALE_RESET_100: {
            // Typical circular counter-clockwise reset arrow icon
            float r = 5.2f;
            float acy = cy + 0.6f;
            g.DrawArc(&whitePen, cx - r, acy - r, r * 2.0f, r * 2.0f, 270.0f, 285.0f);
            PointF head[3] = {
                PointF(cx - 3.8f, acy - r),
                PointF(cx + 0.6f, acy - r - 2.9f),
                PointF(cx + 0.6f, acy - r + 2.9f)
            };
            g.FillPolygon(&whiteBrush, head, 3);
            break;
        }
    }
}

void PepperSnapDaemon::DrawHoverBubbleTooltip(Graphics& g, const DockButton& b, int screenW) {
    if (b.tooltip.empty()) return;
    FontFamily ff(L"Segoe UI");
    Font tipFont(&ff, 11.5f, FontStyleBold, UnitPixel);

    std::vector<std::wstring> lines;
    size_t start = 0;
    while (start <= b.tooltip.size()) {
        size_t nl = b.tooltip.find(L'\n', start);
        if (nl == std::wstring::npos) {
            lines.push_back(b.tooltip.substr(start));
            break;
        }
        lines.push_back(b.tooltip.substr(start, nl - start));
        start = nl + 1;
    }

    float maxLineW = 0.0f;
    for (const auto& ln : lines) {
        RectF measured;
        g.MeasureString(ln.c_str(), -1, &tipFont, PointF(0, 0), &measured);
        if (measured.Width > maxLineW) maxLineW = measured.Width;
    }

    float bw = std::ceil(maxLineW) + 18.0f;
    float bh = 8.0f + (float)lines.size() * 18.0f;
    float bx = 0.0f, by = 0.0f;
    PointF tail[3];

    // Place bubble above or below horizontal button in the 3-row toolbar
    bool placeAbove = (b.rect.top - bh - 10.0f > 8.0f);
    float midX = (b.rect.left + b.rect.right) * 0.5f;
    bx = std::max(8.0f, std::min((float)screenW - bw - 8.0f, midX - bw * 0.5f));
    by = placeAbove ? (b.rect.top - bh - 8.0f) : (b.rect.bottom + 8.0f);
    if (placeAbove) {
        tail[0] = PointF(midX, (float)b.rect.top - 2.0f);
        tail[1] = PointF(midX - 5.0f, by + bh);
        tail[2] = PointF(midX + 5.0f, by + bh);
    } else {
        tail[0] = PointF(midX, (float)b.rect.bottom + 2.0f);
        tail[1] = PointF(midX - 5.0f, by);
        tail[2] = PointF(midX + 5.0f, by);
    }

    SolidBrush bubbleBg(Color(248, 15, 23, 42));
    Pen bubbleBorder(Color(255, 239, 68, 68), 1.4f);
    g.FillPolygon(&bubbleBg, tail, 3);
    g.FillRectangle(&bubbleBg, bx, by, bw, bh);
    g.DrawRectangle(&bubbleBorder, bx, by, bw, bh);

    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    SolidBrush textBr(Color(255, 248, 250, 252));
    for (size_t i = 0; i < lines.size(); ++i) {
        RectF lineRc(bx, by + 4.0f + (float)i * 18.0f, bw, 18.0f);
        g.DrawString(lines[i].c_str(), -1, &tipFont, lineRc, &sf, &textBr);
    }
}

void PepperSnapDaemon::GetDimensionPillAnchor(bool& outRightAlign, bool& outBottomAlign) const {
    outRightAlign = false;
    outBottomAlign = false;
    if (!hasSelection) return;

    int sx = std::min(selRect.left, selRect.right);
    int sy = std::min(selRect.top, selRect.bottom);
    int sw = std::abs(selRect.right - selRect.left);
    int sh = std::abs(selRect.bottom - selRect.top);

    int anchorX = sx;
    int anchorY = sy;

    switch (dragMode) {
        case DragMode::CreatingSelection:
            anchorX = std::max(0, std::min(vScreenW, (int)dragStartPt.x));
            anchorY = std::max(0, std::min(vScreenH, (int)dragStartPt.y));
            break;
        case DragMode::ResizeBR:
        case DragMode::ResizeR:
        case DragMode::ResizeB:
            anchorX = dragOrigRect.left;
            anchorY = dragOrigRect.top;
            break;
        case DragMode::ResizeBL:
        case DragMode::ResizeL:
            anchorX = dragOrigRect.right;
            anchorY = dragOrigRect.top;
            break;
        case DragMode::ResizeTR:
        case DragMode::ResizeT:
            anchorX = dragOrigRect.left;
            anchorY = dragOrigRect.bottom;
            break;
        case DragMode::ResizeTL:
            anchorX = dragOrigRect.right;
            anchorY = dragOrigRect.bottom;
            break;
        default:
            // When not resizing, keep the size indicator at top-left
            outRightAlign = false;
            outBottomAlign = false;
            return;
    }

    if (sw > 0) {
        outRightAlign = (std::abs(anchorX - (sx + sw)) < std::abs(anchorX - sx));
    } else {
        outRightAlign = (dragMode == DragMode::ResizeBL || dragMode == DragMode::ResizeL || dragMode == DragMode::ResizeTL ||
                         (dragMode == DragMode::CreatingSelection && mousePt.x < dragStartPt.x));
    }

    if (sh > 0) {
        outBottomAlign = (std::abs(anchorY - (sy + sh)) < std::abs(anchorY - sy));
    } else {
        outBottomAlign = (dragMode == DragMode::ResizeTL || dragMode == DragMode::ResizeTR || dragMode == DragMode::ResizeT ||
                          (dragMode == DragMode::CreatingSelection && mousePt.y < dragStartPt.y));
    }
}

void PepperSnapDaemon::BuildDockedHUD() {
    dockButtons.clear();
    if (dockButtons.capacity() < 36) dockButtons.reserve(36);
    customStrokeRect = {0, 0, 0, 0};
    hudBoundsRect = {0, 0, 0, 0};
    bool isResizing = (dragMode >= DragMode::ResizeTL && dragMode <= DragMode::ResizeL);
    if (!hasSelection || dragMode == DragMode::CreatingSelection || dragMode == DragMode::MovingSelection || isResizing) return;

    int sx = std::min(selRect.left, selRect.right);
    int sy = std::min(selRect.top, selRect.bottom);
    int sw = std::abs(selRect.right - selRect.left);
    int sh = std::abs(selRect.bottom - selRect.top);
    if (sw < 1 || sh < 1) return;

    // ------------------------------------------------------------------------
    // Combined 3-Row Right-Aligned Toolbar:
    //   Row 1: Horizontal Tool Bar (keeping exact tool order)
    //   Row 2: Colors and Sizes Tool Bar (+ Custom px Input Box)
    //   Row 3: Drag Button + Undo / Redo / Clear / Search on web / Pin / OCR / Saves / Copy / Close Tool Bar
    // ------------------------------------------------------------------------

    struct ToolEntry { OverlayTool t; const wchar_t* lbl; std::wstring tip; };
    ToolEntry tools[] = {
        { OverlayTool::SelectMove,   L"Sel", FormatLabelWithShortcut(L"Select mode",         hkToolSelect) },
        { OverlayTool::Pen,          L"Pen", FormatLabelWithShortcut(L"Pen",                 hkToolPen) },
        { OverlayTool::Highlighter,  L"Hi",  FormatLabelWithShortcut(L"Stabilo highlighter", hkToolStabilo) },
        { OverlayTool::Line,         L"Lin", FormatLabelWithShortcut(L"Line",                hkToolLine) },
        { OverlayTool::Arrow,        L"Arr", FormatLabelWithShortcut(L"Arrow",               hkToolArrow) },
        { OverlayTool::NumberArrow,  L"1→",  FormatLabelWithShortcut(L"Numbering arrow",     hkToolNumber) },
        { OverlayTool::Rectangle,    L"Box", FormatLabelWithShortcut(L"Square / rectangle",  hkToolRect) },
        { OverlayTool::Ellipse,      L"Cir", FormatLabelWithShortcut(L"Circle / ellipse",    hkToolEllipse) },
        { OverlayTool::TextBox,      L"Txt", FormatLabelWithShortcut(L"Text box",            hkToolText) },
        { OverlayTool::MosaicSquare, L"MSq", FormatLabelWithShortcut(L"Mosaic square",       hkToolMosaicSq,  L" — pixelation level is affected by size selection") },
        { OverlayTool::MosaicCircle, L"MCi", FormatLabelWithShortcut(L"Mosaic circle",       hkToolMosaicCir, L" — pixelation level is affected by size selection") }
    };

    bool hasNumberArrow = false;
    for (const auto& a : annotations) {
        if (a.tool == OverlayTool::NumberArrow) {
            hasNumberArrow = true;
            break;
        }
    }

    const int gap = 3;
    const int rowGap = gap;

    // Row 1 metrics (Tools)
    const int toolBtnW = 32, toolBtnH = 28;
    int totalToolCount = 11 + (hasNumberArrow ? 1 : 0);
    int row1W = totalToolCount * toolBtnW + (totalToolCount - 1) * gap;

    // Row 2 metrics (5 Colors + 4 Stroke Sizes + Custom Size Box equal to 2 toolbar buttons)
    const int colorW = toolBtnW, colorH = toolBtnH;
    const int strokeW = toolBtnW, strokeH = toolBtnH;
    const int customBoxW = toolBtnW * 2 + gap, customBoxH = toolBtnH;
    const int groupGap = gap;
    int colorsTotalW = (int)palette.size() * colorW + ((int)palette.size() - 1) * gap;
    int strokesTotalW = (int)strokeSizes.size() * strokeW + ((int)strokeSizes.size() - 1) * gap;
    int row2W = colorsTotalW + groupGap + strokesTotalW + gap + customBoxW;
    const int row2H = toolBtnH;

    // Row 3 metrics (Drag Handle + Undo/Redo/Clear/Search on web/Pin/OCR/SaveAs/Save/Copy/Close)
    std::wstring copyBtnTip = FormatLabelWithShortcut(L"Copy to clipboard", hkActCopy);
    struct ActEntry { int id; int w; const wchar_t* lbl; std::wstring tip; bool primary; };
    ActEntry acts[] = {
        { DBTN_ACT_DRAG_HUD, toolBtnW, L"", L"Drag to move toolbar (resets when selection moves)", false },
        { DBTN_ACT_UNDO,     toolBtnW, L"", FormatLabelWithShortcut(L"Undo",                     hkActUndo),    false },
        { DBTN_ACT_REDO,     toolBtnW, L"", FormatLabelWithShortcut(L"Redo",                     hkActRedo),    false },
        { DBTN_ACT_CLEAR,    toolBtnW, L"", FormatLabelWithShortcut(L"Clear all annotations",    hkActClear),   false },
        { DBTN_ACT_SEARCH_GOOGLE, toolBtnW, L"", FormatLabelWithShortcut(L"Search on web",       hkActSearchGoogle) + L"\nShift + click to translate", false },
        { DBTN_ACT_PIN,      toolBtnW, L"", FormatLabelWithShortcut(L"Pin on top",               hkActPin),     false },
        { DBTN_ACT_OCR,      toolBtnW, L"", FormatLabelWithShortcut(L"Extract text with OCR",    hkActOcr),     false },
        { DBTN_ACT_SAVE_AS,  toolBtnW, L"", FormatLabelWithShortcut(L"Save as JPG/PNG/WEBP/BMP", hkActSaveAs),  false },
        { DBTN_ACT_SAVE,     toolBtnW, L"", FormatLabelWithShortcut(L"Quick save",               hkActSave),    false },
        { DBTN_ACT_COPY,     toolBtnW, L"", copyBtnTip,                                                         true },
        { DBTN_ACT_CLOSE,    toolBtnW, L"", L"Close overlay (Esc)", false }
    };
    const size_t actCount = sizeof(acts) / sizeof(acts[0]);
    const int actBtnH = 28;
    int row3W = 0;
    for (size_t i = 0; i < actCount; ++i) {
        row3W += acts[i].w + (i > 0 ? gap : 0);
    }

    int totalHudW = std::max(row1W, std::max(row2W, row3W));
    int totalHudH = toolBtnH + rowGap + row2H + rowGap + actBtnH; // 28 + 4 + 22 + 4 + 28 = 86px

    int rightX = 0;
    int topY = 0;

    if (hasCustomHudPos) {
        rightX = std::max(totalHudW + 6, std::min(vScreenW - 6, (int)customHudPos.x));
        topY   = std::max(6, std::min(vScreenH - totalHudH - 6, (int)customHudPos.y));
    } else {
        // Align right to selection's right edge (clamped to screen bounds)
        rightX = std::max(totalHudW + 8, std::min(vScreenW - 8, sx + sw));

        bool pillRight = false, pillBottom = false;
        GetDimensionPillAnchor(pillRight, pillBottom);
        int bottomExtra = (pillRight && pillBottom) ? 36 : 0;
        int topExtra    = (pillRight && !pillBottom) ? 36 : 0;

        bool hasSpaceBelow = (sy + sh + 8 + bottomExtra + totalHudH <= vScreenH - 6);
        bool hasSpaceAbove = (sy - 8 - topExtra - totalHudH >= 6);

        if (hasSpaceBelow) {
            // Snap below bottom-right corner
            topY = sy + sh + 8 + bottomExtra;
        } else if (hasSpaceAbove) {
            // No space below bottom side -> snap to top-right corner
            topY = sy - 8 - topExtra - totalHudH;
        } else {
            // Fallback when no space on bottom or top -> put inside bottom-right of selection window
            rightX = std::max(totalHudW + 8, std::min(vScreenW - 8, sx + sw - 8));
            topY   = std::max(6, std::min(vScreenH - totalHudH - 6, sy + sh - totalHudH - 8));
        }
    }

    hudBoundsRect = { rightX - totalHudW, topY, rightX, topY + totalHudH };

    // --- Row 1: Horizontal Tools Strip (Right-aligned to rightX, preserving tool order) ---
    int r1X = rightX - row1W;
    int r1Y = topY;
    for (int i = 0; i < 11; ++i) {
        DockButton b;
        b.id = DBTN_TOOL_BASE + i;
        b.rect = { r1X, r1Y, r1X + toolBtnW, r1Y + toolBtnH };
        b.label = tools[i].lbl;
        if (tools[i].t == OverlayTool::NumberArrow) {
            b.tooltip = FormatLabelWithShortcut(L"Numbering arrow", hkToolNumber) + L" — next: " + std::to_wstring(nextStepNum);
        } else {
            b.tooltip = tools[i].tip;
        }
        b.isTool = true;
        b.tool = tools[i].t;
        dockButtons.push_back(b);
        r1X += toolBtnW + gap;

        if (tools[i].t == OverlayTool::NumberArrow && hasNumberArrow) {
            DockButton rb;
            rb.id = DBTN_ACT_RESET_NUM;
            rb.rect = { r1X, r1Y, r1X + toolBtnW, r1Y + toolBtnH };
            rb.label = L"↺1";
            rb.tooltip = FormatLabelWithShortcut(L"Reset numbering arrow counter to 1", hkToolResetNum) + L" (next: " + std::to_wstring(nextStepNum) + L")";
            rb.isTool = false;
            dockButtons.push_back(rb);
            r1X += toolBtnW + gap;
        }
    }

    // --- Row 2: Colors and Sizes Toolbar (Right-aligned to rightX) ---
    int r2X = rightX - row2W;
    int r2Y = topY + toolBtnH + rowGap;
    const wchar_t* colorNames[5] = {
        L"Red (#EF4444)",
        L"Green (#22C55E)",
        L"Blue (#3B82F6)",
        L"White (#FFFFFF)",
        L"Black (#000000)"
    };
    for (size_t i = 0; i < palette.size(); ++i) {
        DockButton b;
        b.id = DBTN_COLOR_BASE + (int)i;
        b.rect = { r2X, r2Y, r2X + colorW, r2Y + colorH };
        b.isColor = true;
        b.swatchColor = palette[i];
        b.tooltip = colorNames[i];
        dockButtons.push_back(b);
        r2X += colorW + (i + 1 < palette.size() ? gap : 0);
    }

    r2X += groupGap;
    const wchar_t* strokeLabels[4] = { L"S", L"M", L"L", L"XL" };
    const wchar_t* strokeTips[4] = { L"Small size (2px)", L"Medium size (4px)", L"Large size (8px)", L"Extra large size (14px)" };
    for (size_t i = 0; i < strokeSizes.size(); ++i) {
        DockButton b;
        b.id = DBTN_STROKE_BASE + (int)i;
        b.rect = { r2X, r2Y, r2X + strokeW, r2Y + strokeH };
        b.isStroke = true;
        b.strokeVal = strokeSizes[i];
        b.label = strokeLabels[i];
        b.tooltip = strokeTips[i];
        dockButtons.push_back(b);
        r2X += strokeW + (i + 1 < strokeSizes.size() ? gap : 0);
    }

    r2X += gap;
    customStrokeRect = { r2X, r2Y, r2X + customBoxW, r2Y + customBoxH };

    // --- Row 3: Drag Button + Options, Pin, OCR, Saves, Copy & Close Toolbar (Right-aligned to rightX) ---
    int r3X = rightX - row3W;
    int r3Y = r2Y + row2H + rowGap;
    for (size_t i = 0; i < actCount; ++i) {
        DockButton b;
        b.id = acts[i].id;
        b.rect = { r3X, r3Y, r3X + acts[i].w, r3Y + actBtnH };
        b.label = acts[i].lbl;
        b.tooltip = acts[i].tip;
        b.isPrimaryAction = acts[i].primary;
        dockButtons.push_back(b);
        r3X += acts[i].w + gap;
    }
}

DragMode PepperSnapDaemon::HitTestSelectionHandles(int mx, int my) const {
    if (!hasSelection) return DragMode::None;
    POINT handles[8];
    GetSelectionHandlePoints(handles);
    const int r = 7;

    auto nearPt = [&](POINT p) {
        return std::abs(mx - p.x) <= r && std::abs(my - p.y) <= r;
    };

    // Prioritize corner handles first, then edge midpoint handles
    if (nearPt(handles[0])) return DragMode::ResizeTL;
    if (nearPt(handles[2])) return DragMode::ResizeTR;
    if (nearPt(handles[4])) return DragMode::ResizeBR;
    if (nearPt(handles[6])) return DragMode::ResizeBL;
    if (nearPt(handles[1])) return DragMode::ResizeT;
    if (nearPt(handles[3])) return DragMode::ResizeR;
    if (nearPt(handles[5])) return DragMode::ResizeB;
    if (nearPt(handles[7])) return DragMode::ResizeL;
    return DragMode::None;
}


// ----------------------------------------------------------------------------
// Overlay Actions (Copy, Save, Save As, Undo/Redo)
// ----------------------------------------------------------------------------

void PepperSnapDaemon::PushUndo() {
    undoStack.push_back(annotations);
    if (undoStack.size() > 40) undoStack.erase(undoStack.begin());
    redoStack.clear();
}

void PepperSnapDaemon::Undo() {
    CommitActiveTextBox();
    if (undoStack.empty()) return;
    redoStack.push_back(annotations);
    annotations = undoStack.back();
    undoStack.pop_back();
    RecalcNextStepNum();
    ClearAnnotationSelection();
    if (hOverlayWnd) InvalidateRect(hOverlayWnd, nullptr, FALSE);
}

void PepperSnapDaemon::Redo() {
    CommitActiveTextBox();
    if (redoStack.empty()) return;
    undoStack.push_back(annotations);
    annotations = redoStack.back();
    redoStack.pop_back();
    RecalcNextStepNum();
    ClearAnnotationSelection();
    if (hOverlayWnd) InvalidateRect(hOverlayWnd, nullptr, FALSE);
}

void PepperSnapDaemon::ActionCopyAndClose(WebOpenTarget webTarget) {
    // The overlay goes off the screen before the render, the encode and the
    // clipboard handoff - a third of a second of straight CPU work on a full-size
    // region. Nothing below needs the overlay window live, only the daemon state
    // the bitmap is cut from (see HideOverlayForPendingWork).
    bool keepOverlayOpen = (webTarget == WebOpenTarget::None && IsEditingOpenedImage() && keepEditWindowOpen);
    bool hidOverlay = !keepOverlayOpen && HideOverlayForPendingWork();

    Bitmap* bmp = RenderCroppedRegionBitmap();
    if (!bmp) {
        RestoreOverlayAfterPendingWork(hidOverlay);
        return;
    }
    int w = (int)bmp->GetWidth();
    int h = (int)bmp->GetHeight();

    if (!keepOverlayOpen) {
        CloseRegionSnipOverlay();
    }
    CopyBitmapToClipboard(bmp);

    std::wstring savedNote;
    if (autoSaveOnCopy) {
        CreateDirectoryW(saveFolder.c_str(), nullptr);
        std::wstring copyExt = GetFormatExtension(copyFormat);
        std::wstring full = saveFolder + L"\\" + NextFreeCaptureStem(copyExt) + copyExt;
        if (SaveBitmapToPath(bmp, full)) {
            lastSavedFilePath = full;
            savedNote = L"\nAuto-saved to: " + full;
        }
    }
    delete bmp;
    if (keepOverlayOpen) {
        SaveSettings();
        if (hOverlayWnd && IsWindow(hOverlayWnd)) {
            InvalidateRect(hOverlayWnd, nullptr, FALSE);
        }
    }
    if (webTarget == WebOpenTarget::Translate) {
        OpenGoogleTranslateInBrowser(translateTargetLang);
        ShowTrayToast(
            L"Directing to Google Translate",
            L"Image copied to clipboard, Press Ctrl + V once the page loaded"
        );
    } else if (webTarget == WebOpenTarget::Lens) {
        OpenGoogleLensInBrowser();
        ShowTrayToast(
            L"Directing to Google image search",
            L"Image copied to clipboard, Press Ctrl + V once the page loaded"
        );
    } else {
        ShowTrayToast(
            L"Copied to clipboard (" + std::to_wstring(w) + L"×" + std::to_wstring(h) + L" px)",
            L"Ready to paste (Ctrl+V)." + savedNote
        );
    }
}

void PepperSnapDaemon::ActionCopyPixelColorAndClose() {
    if (!frozenDesktopBmp) return;
    int mx = std::max(0, std::min(vScreenW - 1, (int)mousePt.x));
    int my = std::max(0, std::min(vScreenH - 1, (int)mousePt.y));
    BYTE r = 0, g = 0, b = 0;
    if (brightDibPixels) {
        DWORD raw = brightDibPixels[(size_t)my * vScreenW + mx];
        b = (BYTE)(raw & 0xFF);
        g = (BYTE)((raw >> 8) & 0xFF);
        r = (BYTE)((raw >> 16) & 0xFF);
    } else {
        Color pxCol;
        frozenDesktopBmp->GetPixel(mx, my, &pxCol);
        r = pxCol.GetR();
        g = pxCol.GetG();
        b = pxCol.GetB();
    }

    wchar_t hexBuf[32];
    swprintf_s(hexBuf, L"#%02X%02X%02X", r, g, b);
    std::wstring rgbStr(hexBuf);

    HWND hClipOwner = hOverlayWnd ? hOverlayWnd : hTrayWnd;
    if (OpenClipboard(hClipOwner)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (rgbStr.size() + 1) * sizeof(wchar_t));
        if (hMem) {
            wchar_t* dst = (wchar_t*)GlobalLock(hMem);
            if (dst) {
                memcpy(dst, rgbStr.c_str(), (rgbStr.size() + 1) * sizeof(wchar_t));
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            }
        }
        CloseClipboard();
    }

    CloseRegionSnipOverlay();

    wchar_t msgBuf[128];
    swprintf_s(msgBuf, L"RGB: %s (%d, %d, %d) copied to clipboard.",
               hexBuf, r, g, b);
    ShowTrayToast(L"Copied RGB code: " + rgbStr, msgBuf);
}

void PepperSnapDaemon::ActionQuickSaveAndClose() {
    // Down first, then render and encode: both are slow enough to read as a
    // frozen toolbar (see ActionCopyAndClose).
    bool keepOverlayOpen = IsEditingOpenedImage() && keepEditWindowOpen;
    bool hidOverlay = !keepOverlayOpen && HideOverlayForPendingWork();

    Bitmap* bmp = RenderCroppedRegionBitmap();
    if (!bmp) {
        RestoreOverlayAfterPendingWork(hidOverlay);
        return;
    }
    if (!keepOverlayOpen) {
        CloseRegionSnipOverlay();
    }
    CreateDirectoryW(saveFolder.c_str(), nullptr);
    std::wstring quickExt = GetFormatExtension(regionFormat);
    std::wstring full = saveFolder + L"\\" + NextFreeCaptureStem(quickExt) + quickExt;
    if (SaveBitmapToPath(bmp, full)) {
        lastSavedFilePath = full;
        ShowTrayToast(
            L"Capture saved",
            L"Saved to: " + full + L"\n(click to reveal in Explorer)"
        );
    }
    delete bmp;
    if (keepOverlayOpen) {
        SaveSettings();
        if (hOverlayWnd && IsWindow(hOverlayWnd)) {
            InvalidateRect(hOverlayWnd, nullptr, FALSE);
        }
    }
}

void PepperSnapDaemon::ActionSaveAsAndClose() {
    Bitmap* bmp = RenderCroppedRegionBitmap();
    if (!bmp) return;

    StopOverlayFramePlayback();
    isSavingAsModalOpen = true;

    HWND hSaveOwner = (hOverlayWnd && IsWindow(hOverlayWnd)) ? hOverlayWnd : hTrayWnd;
    if (hOverlayWnd && IsWindow(hOverlayWnd)) {
        SetForegroundWindow(hOverlayWnd);
    }

    std::vector<WCHAR> szFile(4096, L'\0');
    std::wstring regionSaveExt = GetFormatExtension(regionFormat);
    std::wstring defName = NextFreeCaptureStem(regionSaveExt) + regionSaveExt;
    wcsncpy_s(szFile.data(), szFile.size(), defName.c_str(), _TRUNCATE);

    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hSaveOwner;
    ofn.lpstrInitialDir = saveFolder.c_str();
    ofn.lpstrFile = szFile.data();
    ofn.nMaxFile = (DWORD)szFile.size();
    ofn.lpstrFilter = L"JPEG image (*.jpg)\0*.jpg\0PNG image (*.png)\0*.png\0WebP image (*.webp)\0*.webp\0BMP bitmap (*.bmp)\0*.bmp\0";
    ofn.nFilterIndex = (DWORD)regionFormat + 1;
    const wchar_t* defExts[4] = { L"jpg", L"png", L"webp", L"bmp" };
    ofn.lpstrDefExt = defExts[(int)regionFormat & 3];
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER;

    BOOL savedOk = GetSaveFileNameW(&ofn);
    isSavingAsModalOpen = false;

    if (savedOk) {
        // Same reasoning as ActionCopyAndClose: the encode should not run with
        // the overlay still up. Here the dialog has already answered the click,
        // but the window still outlives it by the length of the write.
        bool keepOverlayOpen = IsEditingOpenedImage() && keepEditWindowOpen;
        if (!keepOverlayOpen) {
            CloseRegionSnipOverlay();
        }
        if (SaveBitmapToPath(bmp, szFile.data())) {
            lastSavedFilePath = szFile.data();
            ShowTrayToast(L"Saved capture", std::wstring(szFile.data()) + L"\n(click to open in Explorer)");
        }
        delete bmp;
        if (keepOverlayOpen) {
            SaveSettings();
            if (hOverlayWnd && IsWindow(hOverlayWnd)) {
                SetForegroundWindow(hOverlayWnd);
                SetFocus(hOverlayWnd);
                InvalidateRect(hOverlayWnd, nullptr, FALSE);
            }
        }
    } else {
        delete bmp;
        if (hOverlayWnd && IsWindow(hOverlayWnd)) {
            SetForegroundWindow(hOverlayWnd);
            SetFocus(hOverlayWnd);
            InvalidateRect(hOverlayWnd, nullptr, FALSE);
        }
    }
}


// ----------------------------------------------------------------------------
// Overlay Window Rendering & Procedure (With Live Pixelation & Live Text Box)
// ----------------------------------------------------------------------------

static void RenderOverlayWindow(HWND, HDC hdc) {
    int W = g_Daemon.vScreenW;
    int H = g_Daemon.vScreenH;
    if (W <= 0 || H <= 0) return;

    g_Daemon.BuildDockedHUD();

    bool tempBackBuffer = false;
    HDC memDC = g_Daemon.hBackBufferDC;
    HBITMAP memBmp = nullptr;
    HGDIOBJ oldBmp = nullptr;
    if (!memDC) {
        tempBackBuffer = true;
        memDC = CreateCompatibleDC(hdc);
        memBmp = CreateCompatibleBitmap(hdc, W, H);
        oldBmp = SelectObject(memDC, memBmp);
    }

    // Perform all hardware GDI BitBlts + in-place DIB Mosaic pixelation BEFORE constructing GDI+ Graphics(memDC)
    if (g_Daemon.hDimmedDesktopDC) {
        BitBlt(memDC, 0, 0, W, H, g_Daemon.hDimmedDesktopDC, 0, 0, SRCCOPY);
    }
    if (!g_Daemon.hasSelection && g_Daemon.hasCtrlHoverWindow && g_Daemon.hBrightDesktopDC) {
        int wx0 = std::max(0, std::min(W, (int)g_Daemon.ctrlHoverWindowRect.left));
        int wy0 = std::max(0, std::min(H, (int)g_Daemon.ctrlHoverWindowRect.top));
        int ww0 = std::min(W - wx0, (int)(g_Daemon.ctrlHoverWindowRect.right - g_Daemon.ctrlHoverWindowRect.left));
        int wh0 = std::min(H - wy0, (int)(g_Daemon.ctrlHoverWindowRect.bottom - g_Daemon.ctrlHoverWindowRect.top));
        if (ww0 >= 1 && wh0 >= 1) {
            BitBlt(memDC, wx0, wy0, ww0, wh0, g_Daemon.hBrightDesktopDC, wx0, wy0, SRCCOPY);
        }
    }
    if (g_Daemon.hasSelection && g_Daemon.hBrightDesktopDC) {
        int sx0 = std::max(0, std::min(W, (int)std::min(g_Daemon.selRect.left, g_Daemon.selRect.right)));
        int sy0 = std::max(0, std::min(H, (int)std::min(g_Daemon.selRect.top, g_Daemon.selRect.bottom)));
        int sw0 = std::min(W - sx0, (int)std::abs(g_Daemon.selRect.right - g_Daemon.selRect.left));
        int sh0 = std::min(H - sy0, (int)std::abs(g_Daemon.selRect.bottom - g_Daemon.selRect.top));
        if (sw0 >= 1 && sh0 >= 1) {
            POINT polyPtsRgn[4];
            int polyCountRgn = g_Daemon.hasCustomQuad ? g_Daemon.GetSelectionPolygonPoints(polyPtsRgn) : 0;
            HRGN hShapeRgn = nullptr;
            if (g_Daemon.hasEllipseSelection) {
                hShapeRgn = CreateEllipticRgn(sx0, sy0, sx0 + sw0, sy0 + sh0);
            } else if (g_Daemon.hasCustomQuad && polyCountRgn >= 3) {
                hShapeRgn = CreatePolygonRgn(polyPtsRgn, polyCountRgn, WINDING);
            }
            if (hShapeRgn) {
                SelectClipRgn(memDC, hShapeRgn);
            }
            BitBlt(memDC, sx0, sy0, sw0, sh0, g_Daemon.hBrightDesktopDC, sx0, sy0, SRCCOPY);
            if (hShapeRgn) {
                SelectClipRgn(memDC, nullptr);
            }
            if (g_Daemon.backBufferPixels) {
                GdiFlush();
                bool appliedAnyMosaic = false;
                auto applyMosaicClipped = [&](const Annotation& ma) {
                    int px = (int)std::min(ma.startPt.x, ma.endPt.x);
                    int py = (int)std::min(ma.startPt.y, ma.endPt.y);
                    int pw = (int)std::abs(ma.endPt.x - ma.startPt.x);
                    int ph = (int)std::abs(ma.endPt.y - ma.startPt.y);
                    if (px + pw <= sx0 || px >= sx0 + sw0 || py + ph <= sy0 || py >= sy0 + sh0) return;
                    DWORD* subBase = g_Daemon.backBufferPixels + (size_t)sy0 * W + sx0;
                    ApplyPixelateFilterRawBuffer(
                        subBase, sw0, sh0, W * 4,
                        px - sx0, py - sy0, pw, ph,
                        PepperSnapDaemon::GetMosaicBlockSize(ma.strokeWidth),
                        ma.tool == OverlayTool::MosaicCircle
                    );
                    appliedAnyMosaic = true;
                };
                for (const auto& ann : g_Daemon.annotations) {
                    if (ann.tool == OverlayTool::MosaicSquare || ann.tool == OverlayTool::MosaicCircle) {
                        applyMosaicClipped(ann);
                    }
                }
                if (g_Daemon.dragMode == DragMode::DrawingAnnotation &&
                    (g_Daemon.draftAnn.tool == OverlayTool::MosaicSquare || g_Daemon.draftAnn.tool == OverlayTool::MosaicCircle)) {
                    applyMosaicClipped(g_Daemon.draftAnn);
                }
                if (appliedAnyMosaic && hShapeRgn && g_Daemon.hDimmedDesktopDC) {
                    HRGN hBoxRgn = CreateRectRgn(sx0, sy0, sx0 + sw0, sy0 + sh0);
                    if (hBoxRgn) {
                        CombineRgn(hBoxRgn, hBoxRgn, hShapeRgn, RGN_DIFF);
                        SelectClipRgn(memDC, hBoxRgn);
                        BitBlt(memDC, sx0, sy0, sw0, sh0, g_Daemon.hDimmedDesktopDC, sx0, sy0, SRCCOPY);
                        SelectClipRgn(memDC, nullptr);
                        DeleteObject(hBoxRgn);
                    }
                }
            }
            if (hShapeRgn) {
                DeleteObject(hShapeRgn);
            }
        }
    }

    {
        Graphics g(memDC);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

        if (!g_Daemon.hDimmedDesktopDC) {
            if (g_Daemon.frozenDesktopBmp) {
                g.DrawImage(g_Daemon.frozenDesktopBmp, 0, 0, W, H);
            }
            SolidBrush scrimBrush(Color(135, 9, 13, 22));
            g.FillRectangle(&scrimBrush, 0, 0, W, H);
        }

        FontFamily ff(L"Segoe UI");
        Font hudFont(&ff, 11.5f, FontStyleBold, UnitPixel);
        Font smallFont(&ff, 11.0f, FontStyleRegular, UnitPixel);
        Font monoFont(L"Consolas", 11.5f, FontStyleBold, UnitPixel);
        SolidBrush whiteBrush(Color(255, 248, 250, 252));
        SolidBrush mutedBrush(Color(255, 148, 163, 184));

        if (g_Daemon.hasSelection) {
            int sx = std::max(0, std::min(W, (int)std::min(g_Daemon.selRect.left, g_Daemon.selRect.right)));
            int sy = std::max(0, std::min(H, (int)std::min(g_Daemon.selRect.top, g_Daemon.selRect.bottom)));
            int sw = std::min(W - sx, (int)std::abs(g_Daemon.selRect.right - g_Daemon.selRect.left));
            int sh = std::min(H - sy, (int)std::abs(g_Daemon.selRect.bottom - g_Daemon.selRect.top));

            if (sw >= 1 && sh >= 1 && g_Daemon.frozenDesktopBmp) {
                // Clip Highlighter & vector annotations to selection region (ellipse, polygon/triangle, or rectangle)
                if (g_Daemon.hasEllipseSelection) {
                    GraphicsPath ellipseClipPath(FillModeWinding);
                    ellipseClipPath.AddEllipse((REAL)sx, (REAL)sy, (REAL)sw, (REAL)sh);
                    g.SetClip(&ellipseClipPath, CombineModeReplace);
                } else if (g_Daemon.hasCustomQuad) {
                    POINT polyPtsClip[4];
                    int polyCountClip = g_Daemon.GetSelectionPolygonPoints(polyPtsClip);
                    if (polyCountClip >= 3) {
                        PointF quadPtsF[4];
                        for (int i = 0; i < polyCountClip; ++i) {
                            quadPtsF[i] = PointF((REAL)polyPtsClip[i].x, (REAL)polyPtsClip[i].y);
                        }
                        GraphicsPath polyClipPath(FillModeWinding);
                        polyClipPath.AddPolygon(quadPtsF, polyCountClip);
                        g.SetClip(&polyClipPath, CombineModeReplace);
                    } else {
                        g.SetClip(Rect(sx, sy, sw, sh), CombineModeReplace);
                    }
                } else {
                    g.SetClip(Rect(sx, sy, sw, sh), CombineModeReplace);
                }
                const Annotation* draftHi = (g_Daemon.dragMode == DragMode::DrawingAnnotation && g_Daemon.draftAnn.tool == OverlayTool::Highlighter)
                    ? &g_Daemon.draftAnn
                    : nullptr;
                PepperSnapDaemon::RenderHighlighterLayer(
                    g, 0, 0, W, H,
                    g_Daemon.annotations, draftHi, g_Daemon.nonStackingHighlighter
                );

            for (const auto& ann : g_Daemon.annotations) {
                bool isSel = g_Daemon.IsAnnotationSelected(ann.id);
                if (ann.tool == OverlayTool::Highlighter) {
                    if (isSel) {
                        Pen selPen(Color(255, 56, 189, 248), 1.5f);
                        selPen.SetDashStyle(DashStyleDash);
                        float x = std::min(ann.startPt.x, ann.endPt.x) - 6.0f;
                        float y = std::min(ann.startPt.y, ann.endPt.y) - 6.0f;
                        float w = std::max(16.0f, std::abs(ann.endPt.x - ann.startPt.x) + 12.0f);
                        float h = std::max(16.0f, std::abs(ann.endPt.y - ann.startPt.y) + 12.0f);
                        g.DrawRectangle(&selPen, x, y, w, h);
                    }
                } else if (ann.tool != OverlayTool::MosaicSquare && ann.tool != OverlayTool::MosaicCircle) {
                    PepperSnapDaemon::DrawVectorAnnotation(g, ann, 0, 0, isSel, false);
                } else {
                    // Retain temporary outline for Mosaic Rectangle & Mosaic Circle until save so it stays as an indicator!
                    float mx0 = std::min(ann.startPt.x, ann.endPt.x);
                    float my0 = std::min(ann.startPt.y, ann.endPt.y);
                    float mw  = std::abs(ann.endPt.x - ann.startPt.x);
                    float mh  = std::abs(ann.endPt.y - ann.startPt.y);
                    Pen whiteGuide(Color(230, 255, 255, 255), isSel ? 2.0f : 1.6f);
                    Pen dashGuide(isSel ? Color(255, 56, 189, 248) : Color(255, 239, 68, 68), isSel ? 2.0f : 1.6f);
                    dashGuide.SetDashStyle(DashStyleDash);
                    if (ann.tool == OverlayTool::MosaicCircle) {
                        g.DrawEllipse(&whiteGuide, mx0, my0, mw, mh);
                        g.DrawEllipse(&dashGuide, mx0, my0, mw, mh);
                    } else {
                        g.DrawRectangle(&whiteGuide, mx0, my0, mw, mh);
                        g.DrawRectangle(&dashGuide, mx0, my0, mw, mh);
                    }
                    if (isSel) {
                        SolidBrush hFill(Color(255, 255, 255, 255));
                        Pen hBorder(Color(255, 56, 189, 248), 1.5f);
                        g.FillRectangle(&hFill, ann.startPt.x - 4.0f, ann.startPt.y - 4.0f, 8.0f, 8.0f);
                        g.DrawRectangle(&hBorder, ann.startPt.x - 4.0f, ann.startPt.y - 4.0f, 8.0f, 8.0f);
                        g.FillRectangle(&hFill, ann.endPt.x - 4.0f, ann.endPt.y - 4.0f, 8.0f, 8.0f);
                        g.DrawRectangle(&hBorder, ann.endPt.x - 4.0f, ann.endPt.y - 4.0f, 8.0f, 8.0f);
                    }
                }
            }
            if (g_Daemon.dragMode == DragMode::MarqueeSelectingAnnotations && g_Daemon.hasMarqueeBox) {
                int mx0 = std::min(g_Daemon.marqueeRect.left, g_Daemon.marqueeRect.right);
                int my0 = std::min(g_Daemon.marqueeRect.top, g_Daemon.marqueeRect.bottom);
                int mw0 = std::abs(g_Daemon.marqueeRect.right - g_Daemon.marqueeRect.left);
                int mh0 = std::abs(g_Daemon.marqueeRect.bottom - g_Daemon.marqueeRect.top);
                if (mw0 >= 2 || mh0 >= 2) {
                    SolidBrush mqFill(Color(42, 56, 189, 248));
                    Pen mqBorder(Color(235, 56, 189, 248), 1.5f);
                    mqBorder.SetDashStyle(DashStyleDash);
                    g.FillRectangle(&mqFill, mx0, my0, mw0, mh0);
                    g.DrawRectangle(&mqBorder, mx0, my0, mw0, mh0);
                }
            }
            if (g_Daemon.dragMode == DragMode::DrawingAnnotation) {
                if (g_Daemon.draftAnn.tool != OverlayTool::MosaicSquare &&
                    g_Daemon.draftAnn.tool != OverlayTool::MosaicCircle &&
                    g_Daemon.draftAnn.tool != OverlayTool::Highlighter) {
                    PepperSnapDaemon::DrawVectorAnnotation(g, g_Daemon.draftAnn, 0, 0, false, false);
                } else if (g_Daemon.draftAnn.tool == OverlayTool::MosaicSquare ||
                           g_Daemon.draftAnn.tool == OverlayTool::MosaicCircle) {
                    // Temporary outline for Mosaic Rectangle & Mosaic Circle while dragging so it's easy to see
                    float mx0 = std::min(g_Daemon.draftAnn.startPt.x, g_Daemon.draftAnn.endPt.x);
                    float my0 = std::min(g_Daemon.draftAnn.startPt.y, g_Daemon.draftAnn.endPt.y);
                    float mw  = std::abs(g_Daemon.draftAnn.endPt.x - g_Daemon.draftAnn.startPt.x);
                    float mh  = std::abs(g_Daemon.draftAnn.endPt.y - g_Daemon.draftAnn.startPt.y);
                    Pen outerWhite(Color(235, 255, 255, 255), 2.0f);
                    Pen innerDash(Color(255, 239, 68, 68), 2.0f);
                    innerDash.SetDashStyle(DashStyleDash);
                    if (g_Daemon.draftAnn.tool == OverlayTool::MosaicCircle) {
                        g.DrawEllipse(&outerWhite, mx0, my0, mw, mh);
                        g.DrawEllipse(&innerDash, mx0, my0, mw, mh);
                    } else {
                        g.DrawRectangle(&outerWhite, mx0, my0, mw, mh);
                        g.DrawRectangle(&innerDash, mx0, my0, mw, mh);
                    }
                }
            }
            if (g_Daemon.isEditingText) {
                PepperSnapDaemon::DrawVectorAnnotation(g, g_Daemon.editingTextAnn, 0, 0, true, true);
            }
            g.ResetClip();

            // Selection Frame + 8 Resize Handles (Supports circles/ellipses, rectangles, irregular quadrilaterals, and snapped triangles)
            Pen selBorder(Color(255, 239, 68, 68), 1.8f);
            if (g_Daemon.hasEllipseSelection) {
                int drawX = std::max(0, sx);
                int drawY = std::max(0, sy);
                int drawR = std::min(W - 1, sx + sw);
                int drawB = std::min(H - 1, sy + sh);
                g.DrawEllipse(&selBorder, drawX, drawY, std::max(1, drawR - drawX), std::max(1, drawB - drawY));
            } else if (g_Daemon.hasCustomQuad) {
                selBorder.SetLineJoin(LineJoinRound);
                POINT polyPtsDraw[4];
                int polyCountDraw = g_Daemon.GetSelectionPolygonPoints(polyPtsDraw);
                if (polyCountDraw >= 3) {
                    PointF borderPts[4];
                    for (int i = 0; i < polyCountDraw; ++i) {
                        borderPts[i] = PointF(
                            (REAL)std::max(0, std::min(W - 1, (int)polyPtsDraw[i].x)),
                            (REAL)std::max(0, std::min(H - 1, (int)polyPtsDraw[i].y))
                        );
                    }
                    g.DrawPolygon(&selBorder, borderPts, polyCountDraw);
                }
            } else {
                selBorder.SetAlignment(PenAlignmentInset);
                int drawX = std::max(0, sx);
                int drawY = std::max(0, sy);
                int drawR = std::min(W - 1, sx + sw);
                int drawB = std::min(H - 1, sy + sh);
                g.DrawRectangle(&selBorder, drawX, drawY, std::max(1, drawR - drawX), std::max(1, drawB - drawY));
            }

            POINT handles[8];
            g_Daemon.GetSelectionHandlePoints(handles);
            SolidBrush handleFill(Color(255, 255, 255, 255));
            SolidBrush snappedCornerFill(Color(255, 254, 240, 138));
            Pen handleOutline(Color(255, 239, 68, 68), 1.5f);
            for (int hi = 0; hi < 8; ++hi) {
                const auto& pt = handles[hi];
                bool alreadyDrawn = false;
                for (int prev = 0; prev < hi; ++prev) {
                    if (handles[prev].x == pt.x && handles[prev].y == pt.y) {
                        alreadyDrawn = true;
                        break;
                    }
                }
                if (alreadyDrawn) continue;
                bool isStackedCorner = false;
                if ((hi % 2) == 0 && g_Daemon.hasCustomQuad) {
                    int cIdx = hi / 2;
                    for (int other = 0; other < 4; ++other) {
                        if (other != cIdx &&
                            g_Daemon.selQuad[other].x == pt.x &&
                            g_Daemon.selQuad[other].y == pt.y) {
                            isStackedCorner = true;
                            break;
                        }
                    }
                }
                g.FillRectangle((isStackedCorner && g_Daemon.isDraggingSingleCorner) ? &snappedCornerFill : &handleFill, pt.x - 4, pt.y - 4, 8, 8);
                g.DrawRectangle(&handleOutline, pt.x - 4, pt.y - 4, 8, 8);
            }

            // Dynamic Typeable Dimension & Permanent Ratio Pill [ W×H px | ratio | trash icon ] — anchored to the stationary corner when resizing!
            bool pillRight = false, pillBottom = false;
            g_Daemon.GetDimensionPillAnchor(pillRight, pillBottom);
            const int pillH = 28;
            float pillFontSize = std::max(11.0f, (float)pillH * 0.60f);
            Font pillMeasureFont(&ff, pillFontSize, FontStyleRegular, UnitPixel);

            std::wstring dimValStr = g_Daemon.isEditingSize
                ? g_Daemon.editingSizeText
                : (std::to_wstring(sw) + L"\x00D7" + std::to_wstring(sh));
            std::wstring fullDimStr = dimValStr + L" px";
            std::wstring ratioStr = g_Daemon.isEditingRatio
                ? g_Daemon.editingRatioText
                : g_Daemon.FormatSelectionRatioString(sw, sh);

            float dimTextW = PepperSnapDaemon::MeasureMonoPrefixWidth(g, pillMeasureFont, fullDimStr, fullDimStr.size());
            float ratioTextW = PepperSnapDaemon::MeasureMonoPrefixWidth(g, pillMeasureFont, ratioStr, ratioStr.size());
            int sizeBoxW = std::max(64, (int)std::ceil(dimTextW) + 18);
            int ratioBoxW = std::max(38, (int)std::ceil(ratioTextW) + 16);
            int resetBtnW = g_Daemon.hasCustomRatio ? 28 : 0;
            int pillW = sizeBoxW + ratioBoxW + resetBtnW;

            const int outlineGap = 8; // Same distance to selected window outline as the toolbar (8px)
            int pillX = pillRight ? (sx + sw - pillW - 5) : (sx + 5);
            pillX = std::max(5, std::min(W - pillW - 5, pillX));

            int pillY = 0;
            if (!pillBottom) {
                pillY = (sy >= pillH + outlineGap + 4) ? (sy - outlineGap - pillH) : (sy + 6);
            } else {
                pillY = (sy + sh + outlineGap + pillH <= H - 4) ? (sy + sh + outlineGap) : (sy + sh - pillH - 6);
            }
            pillY = std::max(4, std::min(H - pillH - 4, pillY));

            RECT candidatePill = { pillX, pillY, pillX + pillW, pillY + pillH };
            RECT overlapRc;
            if (IntersectRect(&overlapRc, &candidatePill, &g_Daemon.hudBoundsRect)) {
                if (!pillBottom && pillY < sy) {
                    pillY = sy + 6;
                } else if (pillBottom && pillY > sy + sh) {
                    pillY = std::max(4, sy + sh - pillH - 6);
                }
            }
            g_Daemon.dimPillRect = { pillX, pillY, pillX + sizeBoxW, pillY + pillH };
            g_Daemon.ratioPillRect = { pillX + sizeBoxW, pillY, pillX + sizeBoxW + ratioBoxW, pillY + pillH };
            if (g_Daemon.hasCustomRatio) {
                g_Daemon.ratioResetBtnRect = { pillX + sizeBoxW + ratioBoxW, pillY, pillX + pillW, pillY + pillH };
            } else {
                g_Daemon.ratioResetBtnRect = { 0, 0, 0, 0 };
            }
            bool pillHovered = PtInRect(&g_Daemon.dimPillRect, g_Daemon.mousePt);
            bool ratioHovered = PtInRect(&g_Daemon.ratioPillRect, g_Daemon.mousePt);
            bool resetHovered = g_Daemon.hasCustomRatio && PtInRect(&g_Daemon.ratioResetBtnRect, g_Daemon.mousePt);
            bool resetPressed = g_Daemon.ratioResetBtnPressed && resetHovered;

            SolidBrush sizeBg(g_Daemon.isEditingSize ? Color(252, 30, 41, 59) : Color(235, 15, 23, 42));
            SolidBrush ratioBg(g_Daemon.isEditingRatio ? Color(252, 30, 41, 59) : Color(235, 15, 23, 42));
            g.FillRectangle(&sizeBg, pillX, pillY, sizeBoxW, pillH);
            g.FillRectangle(&ratioBg, pillX + sizeBoxW, pillY, ratioBoxW, pillH);
            if (g_Daemon.hasCustomRatio) {
                Color resetBgCol = resetPressed ? Color(255, 71, 85, 105) : (resetHovered ? Color(255, 51, 65, 85) : Color(235, 15, 23, 42));
                SolidBrush resetBg(resetBgCol);
                g.FillRectangle(&resetBg, pillX + sizeBoxW + ratioBoxW, pillY, resetBtnW, pillH);
            }

            Pen divPen(Color(190, 71, 85, 105), 1.2f);
            g.DrawLine(&divPen, pillX + sizeBoxW, pillY + 4, pillX + sizeBoxW, pillY + pillH - 4);

            Pen pillBorder(Color(160, 71, 85, 105), 1.4f);
            g.DrawRectangle(&pillBorder, pillX, pillY, pillW, pillH);
            if (g_Daemon.isEditingSize || pillHovered) {
                Pen hiBorder(g_Daemon.isEditingSize ? Color(255, 56, 189, 248) : Color(255, 239, 68, 68), 1.4f);
                g.DrawRectangle(&hiBorder, pillX, pillY, sizeBoxW, pillH);
            }
            if (g_Daemon.isEditingRatio || ratioHovered) {
                Pen hiBorder(g_Daemon.isEditingRatio ? Color(255, 56, 189, 248) : Color(255, 239, 68, 68), 1.4f);
                g.DrawRectangle(&hiBorder, pillX + sizeBoxW, pillY, ratioBoxW, pillH);
            }
            if (g_Daemon.hasCustomRatio && resetHovered) {
                Pen hiBorder(Color(255, 239, 68, 68), 1.4f);
                g.DrawRectangle(&hiBorder, pillX + sizeBoxW + ratioBoxW, pillY, resetBtnW, pillH);
            }

            PepperSnapDaemon::DrawEditablePillText(
                g, g_Daemon.dimPillRect,
                dimValStr, L" px",
                g_Daemon.isEditingSize, g_Daemon.sizeCaretPos, g_Daemon.sizeSelAnchor
            );
            Color customRatioRed(255, 239, 68, 68);
            bool showRedRatioText = g_Daemon.hasCustomRatio &&
                                    !g_Daemon.isEditingRatio &&
                                    !(g_Daemon.activeSelectionRatioW > 0 && g_Daemon.activeSelectionRatioH > 0);
            PepperSnapDaemon::DrawEditablePillText(
                g, g_Daemon.ratioPillRect,
                ratioStr, L"",
                g_Daemon.isEditingRatio, g_Daemon.ratioCaretPos, g_Daemon.ratioSelAnchor,
                showRedRatioText ? &customRatioRed : nullptr
            );
            if (g_Daemon.hasCustomRatio) {
                DockButton resetIconBtn;
                resetIconBtn.id = DBTN_ACT_CLEAR;
                resetIconBtn.isTool = false;
                RectF resetRf(
                    (float)g_Daemon.ratioResetBtnRect.left,
                    (float)g_Daemon.ratioResetBtnRect.top,
                    (float)(g_Daemon.ratioResetBtnRect.right - g_Daemon.ratioResetBtnRect.left),
                    (float)(g_Daemon.ratioResetBtnRect.bottom - g_Daemon.ratioResetBtnRect.top)
                );
                PepperSnapDaemon::DrawDockButtonIcon(g, resetIconBtn, resetRf);
            }
        }

        // Docked Toolbars
        StringFormat centerFmt;
        centerFmt.SetAlignment(StringAlignmentCenter);
        centerFmt.SetLineAlignment(StringAlignmentCenter);

        for (const auto& b : g_Daemon.dockButtons) {
            RectF rf((float)b.rect.left, (float)b.rect.top, (float)(b.rect.right - b.rect.left), (float)(b.rect.bottom - b.rect.top));
            bool hovered = (b.id == g_Daemon.hoveredBtnId);
            bool pressed = (b.id == g_Daemon.pressedBtnId && hovered);

            if (b.isColor) {
                SolidBrush swB(b.swatchColor);
                g.FillRectangle(&swB, rf);
                bool actCol = (b.swatchColor.GetValue() == g_Daemon.activeColor.GetValue());
                bool isWhiteSwatch = (b.swatchColor.GetR() > 235 && b.swatchColor.GetG() > 235 && b.swatchColor.GetB() > 235);
                Color borderCol = actCol
                    ? (isWhiteSwatch ? Color(255, 239, 68, 68) : Color(255, 255, 255, 255))
                    : (hovered ? Color(255, 203, 213, 225) : Color(220, 71, 85, 105));
                Pen swP(borderCol, (actCol || pressed) ? 2.2f : 1.0f);
                g.DrawRectangle(&swP, rf.X, rf.Y, rf.Width, rf.Height);
                continue;
            }

            OverlayTool effectiveTool = g_Daemon.GetEffectiveTool();
            bool activeState = (b.isTool && b.tool == effectiveTool) ||
                               (b.isStroke && std::abs(b.strokeVal - g_Daemon.activeStroke) < 0.5f);
            Color bgCol = Color(240, 15, 23, 42);
            if (b.isPrimaryAction || activeState) {
                bgCol = pressed ? Color(255, 185, 28, 28) : (hovered ? Color(255, 220, 38, 38) : Color(255, 239, 68, 68));
            } else if (pressed) {
                bgCol = Color(255, 71, 85, 105);
            } else if (hovered) {
                bgCol = Color(255, 51, 65, 85);
            }

            SolidBrush btnBg(bgCol);
            Pen btnBorder(activeState ? Color(255, 252, 165, 165) : Color(220, 51, 65, 85), 1.0f);
            g.FillRectangle(&btnBg, rf);
            g.DrawRectangle(&btnBorder, rf.X, rf.Y, rf.Width, rf.Height);
            if (b.isStroke) {
                g.DrawString(b.label.c_str(), -1, &hudFont, rf, &centerFmt, &whiteBrush);
            } else {
                PepperSnapDaemon::DrawDockButtonIcon(g, b, rf);
            }
        }

        // Render Typeable Custom Size Input Box on the right side of [XL]
        if (g_Daemon.customStrokeRect.right > g_Daemon.customStrokeRect.left) {
            RectF crf(
                (float)g_Daemon.customStrokeRect.left,
                (float)g_Daemon.customStrokeRect.top,
                (float)(g_Daemon.customStrokeRect.right - g_Daemon.customStrokeRect.left),
                (float)(g_Daemon.customStrokeRect.bottom - g_Daemon.customStrokeRect.top)
            );
            bool cHovered = PtInRect(&g_Daemon.customStrokeRect, g_Daemon.mousePt);
            SolidBrush cBg(g_Daemon.isEditingStroke ? Color(252, 30, 41, 59) : Color(240, 15, 23, 42));
            Pen cBorder(
                g_Daemon.isEditingStroke ? Color(255, 56, 189, 248) : (cHovered ? Color(255, 239, 68, 68) : Color(220, 51, 65, 85)),
                1.3f
            );
            g.FillRectangle(&cBg, crf);
            g.DrawRectangle(&cBorder, crf.X, crf.Y, crf.Width, crf.Height);

            std::wstring cValStr = g_Daemon.isEditingStroke
                ? g_Daemon.editingStrokeText
                : std::to_wstring((int)std::round(g_Daemon.activeStroke));
            PepperSnapDaemon::DrawEditablePillText(
                g, g_Daemon.customStrokeRect,
                cValStr, L" px",
                g_Daemon.isEditingStroke, g_Daemon.strokeCaretPos, g_Daemon.strokeSelAnchor
            );

            if (cHovered && !g_Daemon.isEditingStroke && g_Daemon.hoveredBtnId == -1 && g_Daemon.dragMode == DragMode::None) {
                DockButton fakeTip;
                fakeTip.rect = g_Daemon.customStrokeRect;
                fakeTip.tooltip = L"Custom size — click to type custom size (1–120px)";
                fakeTip.isTool = false;
                PepperSnapDaemon::DrawHoverBubbleTooltip(g, fakeTip, W);
            }
        }

        // Render Floating Speech-Bubble Tooltip when hovering Reset ratio button
        if (g_Daemon.hasSelection && g_Daemon.hasCustomRatio &&
            PtInRect(&g_Daemon.ratioResetBtnRect, g_Daemon.mousePt) &&
            g_Daemon.hoveredBtnId == -1 && g_Daemon.dragMode == DragMode::None) {
            DockButton resetTip;
            resetTip.rect = g_Daemon.ratioResetBtnRect;
            resetTip.tooltip = L"Reset ratio";
            resetTip.isTool = false;
            PepperSnapDaemon::DrawHoverBubbleTooltip(g, resetTip, W);
        }

        // Render Floating Speech-Bubble Tooltip when hovering any button (hidden while dragging)
        if (g_Daemon.hoveredBtnId != -1 && g_Daemon.dragMode == DragMode::None) {
            for (const auto& b : g_Daemon.dockButtons) {
                if (b.id == g_Daemon.hoveredBtnId) {
                    PepperSnapDaemon::DrawHoverBubbleTooltip(g, b, W);
                    break;
                }
            }
        }
    } else {
        if (g_Daemon.hasCtrlHoverWindow) {
            int wx = std::max(0, std::min(W, (int)g_Daemon.ctrlHoverWindowRect.left));
            int wy = std::max(0, std::min(H, (int)g_Daemon.ctrlHoverWindowRect.top));
            int ww = std::min(W - wx, (int)(g_Daemon.ctrlHoverWindowRect.right - g_Daemon.ctrlHoverWindowRect.left));
            int wh = std::min(H - wy, (int)(g_Daemon.ctrlHoverWindowRect.bottom - g_Daemon.ctrlHoverWindowRect.top));
            if (ww >= 1 && wh >= 1) {
                Pen winBorder(Color(255, 239, 68, 68), 2.0f);
                winBorder.SetAlignment(PenAlignmentInset);
                int drawX = std::max(0, wx);
                int drawY = std::max(0, wy);
                int drawR = std::min(W - 1, wx + ww);
                int drawB = std::min(H - 1, wy + wh);
                g.DrawRectangle(&winBorder, drawX, drawY, std::max(1, drawR - drawX), std::max(1, drawB - drawY));

                const int pillH = 28;
                float pillFontSize = std::max(11.0f, (float)pillH * 0.60f);
                Font pillMeasureFont(&ff, pillFontSize, FontStyleRegular, UnitPixel);

                std::wstring dimValStr = std::to_wstring(ww) + L"\x00D7" + std::to_wstring(wh);
                std::wstring fullDimStr = dimValStr + L" px";
                int gVal = PepperSnapDaemon::ComputeIntGcd(ww, wh);
                std::wstring ratioStr = std::to_wstring(ww / gVal) + L":" + std::to_wstring(wh / gVal);

                float dimTextW = PepperSnapDaemon::MeasureMonoPrefixWidth(g, pillMeasureFont, fullDimStr, fullDimStr.size());
                float ratioTextW = PepperSnapDaemon::MeasureMonoPrefixWidth(g, pillMeasureFont, ratioStr, ratioStr.size());
                int sizeBoxW = std::max(64, (int)std::ceil(dimTextW) + 18);
                int ratioBoxW = std::max(38, (int)std::ceil(ratioTextW) + 16);
                int pillW = sizeBoxW + ratioBoxW;

                const int outlineGap = 8;
                int pillX = std::max(5, std::min(W - pillW - 5, wx + 5));
                int pillY = (wy >= pillH + outlineGap + 4) ? (wy - outlineGap - pillH) : (wy + 6);
                pillY = std::max(4, std::min(H - pillH - 4, pillY));

                SolidBrush pillBg(Color(235, 15, 23, 42));
                g.FillRectangle(&pillBg, pillX, pillY, pillW, pillH);
                Pen divPen(Color(190, 71, 85, 105), 1.2f);
                g.DrawLine(&divPen, pillX + sizeBoxW, pillY + 4, pillX + sizeBoxW, pillY + pillH - 4);
                Pen pillBorder(Color(160, 71, 85, 105), 1.4f);
                g.DrawRectangle(&pillBorder, pillX, pillY, pillW, pillH);

                RECT hoverSizeRect = { pillX, pillY, pillX + sizeBoxW, pillY + pillH };
                RECT hoverRatioRect = { pillX + sizeBoxW, pillY, pillX + pillW, pillY + pillH };
                PepperSnapDaemon::DrawEditablePillText(
                    g, hoverSizeRect,
                    dimValStr, L" px", false, 0, 0
                );
                PepperSnapDaemon::DrawEditablePillText(
                    g, hoverRatioRect,
                    ratioStr, L"", false, 0, 0
                );
            }
        } else {
            Pen crossPen(Color(150, 239, 68, 68), 1.0f);
            crossPen.SetDashStyle(DashStyleDash);
            bool shiftHeldCross = (g_Daemon.dragMode == DragMode::None || g_Daemon.pendingShiftColorPick) &&
                                  !PepperSnapDaemon::IsAltModifierHeld() &&
                                  (((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) ||
                                   ((GetKeyState(VK_SHIFT) & 0x8000) != 0) ||
                                   ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0) ||
                                   ((GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0));
            if (shiftHeldCross) {
                GraphicsState stScreenCross = g.Save();
                g.SetClip(Rect(g_Daemon.mousePt.x, g_Daemon.mousePt.y, 1, 1), CombineModeExclude);
                g.DrawLine(&crossPen, g_Daemon.mousePt.x, 0, g_Daemon.mousePt.x, H);
                g.DrawLine(&crossPen, 0, g_Daemon.mousePt.y, W, g_Daemon.mousePt.y);
                g.Restore(stScreenCross);
            } else {
                g.DrawLine(&crossPen, g_Daemon.mousePt.x, 0, g_Daemon.mousePt.x, H);
                g.DrawLine(&crossPen, 0, g_Daemon.mousePt.y, W, g_Daemon.mousePt.y);
            }
        }
    }

    // Magnifier Loupe when aiming, creating a selection, or resizing an existing selection
    bool isResizingSelection =
        (g_Daemon.dragMode >= DragMode::ResizeTL && g_Daemon.dragMode <= DragMode::ResizeL);
    if (!g_Daemon.disableMagnifierWindow && (!g_Daemon.hasSelection || g_Daemon.dragMode == DragMode::CreatingSelection || isResizingSelection)) {
        int mx = g_Daemon.mousePt.x;
        int my = g_Daemon.mousePt.y;
        if (isResizingSelection || g_Daemon.dragMode == DragMode::CreatingSelection) {
            mx = g_Daemon.selectionTargetPt.x;
            my = g_Daemon.selectionTargetPt.y;
        }
        int cMx = std::max(0, std::min(W - 1, mx));
        int cMy = std::max(0, std::min(H - 1, my));
        BYTE pr = 0, pg = 0, pb = 0;
        if (g_Daemon.brightDibPixels) {
            DWORD raw = g_Daemon.brightDibPixels[(size_t)cMy * W + cMx];
            pb = (BYTE)(raw & 0xFF);
            pg = (BYTE)((raw >> 8) & 0xFF);
            pr = (BYTE)((raw >> 16) & 0xFF);
        } else if (g_Daemon.frozenDesktopBmp) {
            Color pxCol;
            g_Daemon.frozenDesktopBmp->GetPixel(cMx, cMy, &pxCol);
            pr = pxCol.GetR();
            pg = pxCol.GetG();
            pb = pxCol.GetB();
        }
        wchar_t hexBuf[64];
        swprintf_s(hexBuf, L"(%d,%d) #%02X%02X%02X", mx, my, pr, pg, pb);

        bool ctrlHeldNow = ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) ||
                           ((GetKeyState(VK_CONTROL) & 0x8000) != 0) ||
                           ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0) ||
                           ((GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0);
        bool shiftHeldNow = ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) ||
                            ((GetKeyState(VK_SHIFT) & 0x8000) != 0) ||
                            ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0) ||
                            ((GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0);
        bool shiftBeforeSelect = (!g_Daemon.hasSelection && shiftHeldNow);

        const wchar_t* hintRow1 = L"Hold Shift and click to pick color";
        const wchar_t* hintRow2 = shiftBeforeSelect
            ? L"Scroll to control magnification"
            : L"Hold Ctrl and click to select window";
        const wchar_t* hintRow3 = shiftBeforeSelect
            ? L"Arrow keys to jump pixel-by-pixel"
            : (ctrlHeldNow ? L"Press Enter to capture window" : L"Hold Alt and drag for circular area");
        const wchar_t* hintRow4 = L"Press Esc to exit";
        const wchar_t* resizeRow1 = L"Press arrow keys to";
        const wchar_t* resizeRow2 = L"jump pixel-by-pixel";
        const wchar_t* resizeRow3 = L"while holding left-click";

        RectF bHex, b1, b2, b3, b4;
        int imgW = 110;
        int imgH = 110;
        int loupeW = imgW + 12;
        int loupeH = imgH + 12;
        if (!isResizingSelection) {
            RectF b2Default, b3Default;
            g.MeasureString(hexBuf, -1, &monoFont, PointF(0.0f, 0.0f), &bHex);
            g.MeasureString(hintRow1, -1, &smallFont, PointF(0.0f, 0.0f), &b1);
            g.MeasureString(hintRow2, -1, &smallFont, PointF(0.0f, 0.0f), &b2);
            g.MeasureString(hintRow3, -1, &smallFont, PointF(0.0f, 0.0f), &b3);
            g.MeasureString(hintRow4, -1, &smallFont, PointF(0.0f, 0.0f), &b4);
            g.MeasureString(L"Hold Ctrl and click to select window", -1, &smallFont, PointF(0.0f, 0.0f), &b2Default);
            g.MeasureString(L"Press Enter to capture window", -1, &smallFont, PointF(0.0f, 0.0f), &b3Default);
            float maxRowW = std::max(
                std::max(std::max(b1.Width, b2.Width), std::max(b3.Width, b4.Width)),
                std::max(b2Default.Width, b3Default.Width)
            );
            int maxHintW = (int)std::ceil(maxRowW) + 4;
            int minHexRowW = (int)std::ceil(bHex.Width) + 44;
            imgW = std::max(120, ((std::max(maxHintW, minHexRowW) + 2) + 1) & ~1);
            imgH = 90;
            loupeW = imgW + 12;
            loupeH = 186;
        } else {
            RectF br1, br2, br3;
            g.MeasureString(resizeRow1, -1, &smallFont, PointF(0.0f, 0.0f), &br1);
            g.MeasureString(resizeRow2, -1, &smallFont, PointF(0.0f, 0.0f), &br2);
            g.MeasureString(resizeRow3, -1, &smallFont, PointF(0.0f, 0.0f), &br3);
            int maxResizeHintW = (int)std::ceil(std::max(br1.Width, std::max(br2.Width, br3.Width))) + 4;
            imgW = std::max(120, ((maxResizeHintW + 2) + 1) & ~1);
            imgH = 110;
            loupeW = imgW + 12;
            loupeH = imgH + 63;
        }

        int lx = (mx + 24 + loupeW < W) ? (mx + 24) : (mx - loupeW - 24);
        int ly = (my + 24 + loupeH < H) ? (my + 24) : (my - loupeH - 24);

        SolidBrush loupeBg(Color(240, 15, 23, 42));
        Pen loupeBorder(Color(255, 239, 68, 68), 1.5f);
        g.FillRectangle(&loupeBg, lx, ly, loupeW, loupeH);

        if (g_Daemon.frozenDesktopBmp) {
            bool overrideGridSnap = (!g_Daemon.hasSelection &&
                                     (g_Daemon.dragMode == DragMode::None || g_Daemon.pendingShiftColorPick) &&
                                     shiftHeldNow && !ctrlHeldNow && !PepperSnapDaemon::IsAltModifierHeld());
            int mag = g_Daemon.hasSelection ? 5 : std::max(1, std::min(210, g_Daemon.loupeMagnification));
            double scaleX = 1.0 / (double)mag;
            double scaleY = 1.0 / (double)mag;
            double subShift = (mag & 1) ? 0.0 : 0.5;

            if (g_Daemon.brightDibPixels && g_Daemon.backBufferPixels && !tempBackBuffer) {
                g.Flush(FlushIntentionSync);
                int dstX0 = lx + 6;
                int dstY0 = ly + 6;
                double halfW = (double)(imgW / 2);
                double halfH = (double)(imgH / 2);
                for (int dy = 0; dy < imgH; ++dy) {
                    int dstY = dstY0 + dy;
                    if (dstY < 0 || dstY >= H) continue;
                    double relY = overrideGridSnap
                        ? (((double)dy + subShift - halfH) * scaleY)
                        : (((double)dy - halfH) * scaleY);
                    double srcYf = (overrideGridSnap || mag <= 1) ? ((double)my + 0.5 + relY) : ((double)my + relY);
                    int sy = std::max(0, std::min(H - 1, (int)std::floor(srcYf)));
                    const DWORD* srcRow = g_Daemon.brightDibPixels + (size_t)sy * W;
                    DWORD* dstRow = g_Daemon.backBufferPixels + (size_t)dstY * W;
                    for (int dx = 0; dx < imgW; ++dx) {
                        int dstX = dstX0 + dx;
                        if (dstX < 0 || dstX >= W) continue;
                        double relX = overrideGridSnap
                            ? (((double)dx + subShift - halfW) * scaleX)
                            : (((double)dx - halfW) * scaleX);
                        double srcXf = (overrideGridSnap || mag <= 1) ? ((double)mx + 0.5 + relX) : ((double)mx + relX);
                        int sx = std::max(0, std::min(W - 1, (int)std::floor(srcXf)));
                        dstRow[dstX] = srcRow[sx];
                    }
                }
            } else {
                GraphicsState st = g.Save();
                g.SetInterpolationMode(InterpolationModeNearestNeighbor);
                g.SetPixelOffsetMode(PixelOffsetModeHalf);
                float srcW = (float)imgW / (float)mag;
                float srcH = (float)imgH / (float)mag;
                float srcXf = (overrideGridSnap || mag <= 1) ? ((float)mx + 0.5f - srcW * 0.5f) : ((float)mx - srcW * 0.5f);
                float srcYf = (overrideGridSnap || mag <= 1) ? ((float)my + 0.5f - srcH * 0.5f) : ((float)my - srcH * 0.5f);
                g.DrawImage(g_Daemon.frozenDesktopBmp, RectF((float)(lx + 6), (float)(ly + 6), (float)imgW, (float)imgH), srcXf, srcYf, srcW, srcH, UnitPixel);
                g.Restore(st);
            }

            Pen centerCross(Color(200, 239, 68, 68), 1.5f);
            int crossX = lx + 6 + imgW / 2;
            int crossY = ly + 6 + imgH / 2;
            if (overrideGridSnap) {
                int pxLeft = crossX - (mag / 2);
                int pxTop  = crossY - (mag / 2);
                GraphicsState stCross = g.Save();
                g.SetClip(Rect(lx + 6, ly + 6, imgW, imgH), CombineModeIntersect);
                g.SetClip(Rect(pxLeft, pxTop, mag, mag), CombineModeExclude);
                g.DrawLine(&centerCross, crossX, ly + 6, crossX, ly + 6 + imgH);
                g.DrawLine(&centerCross, lx + 6, crossY, lx + 6 + imgW, crossY);
                g.Restore(stCross);
            } else {
                g.DrawLine(&centerCross, crossX, ly + 6, crossX, ly + 6 + imgH);
                g.DrawLine(&centerCross, lx + 6, crossY, lx + 6 + imgW, crossY);
            }

            if (!isResizingSelection) {
                g.DrawString(hexBuf, -1, &monoFont, PointF((float)(lx + 7), (float)(ly + 101)), &whiteBrush);
                int stripX = lx + 7 + (int)std::ceil(bHex.Width) + 3;
                int stripRight = lx + 6 + imgW;
                if (stripRight > stripX + 4) {
                    int stripW = stripRight - stripX;
                    int stripY = ly + 102;
                    int stripH = 12;
                    GraphicsState stStrip = g.Save();
                    g.SetSmoothingMode(SmoothingModeNone);
                    g.SetPixelOffsetMode(PixelOffsetModeHalf);
                    SolidBrush pickColorBrush(Color(255, pr, pg, pb));
                    g.FillRectangle(&pickColorBrush, stripX, stripY, stripW, stripH);
                    g.Restore(stStrip);
                }
                g.DrawString(hintRow1, -1, &smallFont, PointF((float)(lx + 7), (float)(ly + 118)), &mutedBrush);
                g.DrawString(hintRow2, -1, &smallFont, PointF((float)(lx + 7), (float)(ly + 134)), &mutedBrush);
                g.DrawString(hintRow3, -1, &smallFont, PointF((float)(lx + 7), (float)(ly + 150)), &mutedBrush);
                g.DrawString(hintRow4, -1, &smallFont, PointF((float)(lx + 7), (float)(ly + 166)), &mutedBrush);
            } else {
                int baseTextY = ly + 6 + imgH + 5;
                g.DrawString(resizeRow1, -1, &smallFont, PointF((float)(lx + 7), (float)baseTextY), &mutedBrush);
                g.DrawString(resizeRow2, -1, &smallFont, PointF((float)(lx + 7), (float)(baseTextY + 16)), &mutedBrush);
                g.DrawString(resizeRow3, -1, &smallFont, PointF((float)(lx + 7), (float)(baseTextY + 32)), &mutedBrush);
            }
        }
        g.DrawRectangle(&loupeBorder, lx, ly, loupeW, loupeH);
    }

    // Render Multi-Frame GIF / WebP Selector Strip ON TOP of custom area and its toolbar!
    if (g_Daemon.HasOverlayMultiFrames()) {
        g_Daemon.RenderOverlayFrameStrip(g, W, H);
    }

    // Render Pop-Up Info when hovering selected custom area (similar to pinned image pop-up info)
    if (g_Daemon.showingCustomAreaHoverTip && g_Daemon.IsHoveringSelectedCustomArea(g_Daemon.mousePt)) {
        const wchar_t* tipText = L"Ctrl + click to drag corner point individually";
        Font tipFont(&ff, 11.5f, FontStyleBold, UnitPixel);
        RectF measured;
        g.MeasureString(tipText, -1, &tipFont, PointF(0.0f, 0.0f), &measured);

        int bw = (int)std::ceil(measured.Width) + 18;
        int bh = 8 + 18;
        int tailH = 7;
        int totalW = bw + 4;
        int totalH = bh + tailH + 4;

        POINT ptCur = g_Daemon.customAreaHoverTipPt;
        bool placeBelow = (ptCur.y + 18 + totalH <= H - 6);
        int winX = std::max(6, std::min(W - totalW - 6, (int)ptCur.x - totalW / 2));
        int winY = placeBelow ? (ptCur.y + 18) : (ptCur.y - totalH - 8);
        float localMidX = (float)std::max(10, std::min(totalW - 10, (int)ptCur.x - winX));

        float rx = (float)winX + 2.0f;
        float ry = (float)winY + (placeBelow ? (float)tailH : 2.0f);
        float rw = (float)bw;
        float rh = (float)bh;

        PointF tail[3];
        if (placeBelow) {
            tail[0] = PointF((float)winX + localMidX, (float)winY + 1.0f);
            tail[1] = PointF((float)winX + localMidX - 5.5f, ry + 1.0f);
            tail[2] = PointF((float)winX + localMidX + 5.5f, ry + 1.0f);
        } else {
            tail[0] = PointF((float)winX + localMidX, ry + rh + (float)tailH - 1.0f);
            tail[1] = PointF((float)winX + localMidX - 5.5f, ry + rh - 1.0f);
            tail[2] = PointF((float)winX + localMidX + 5.5f, ry + rh - 1.0f);
        }

        SolidBrush bubbleBg(Color(248, 15, 23, 42));
        Pen bubbleBorder(Color(255, 239, 68, 68), 1.4f);
        g.FillPolygon(&bubbleBg, tail, 3);
        g.FillRectangle(&bubbleBg, rx, ry, rw, rh);
        g.DrawRectangle(&bubbleBorder, rx, ry, rw, rh);

        StringFormat sf;
        sf.SetAlignment(StringAlignmentCenter);
        sf.SetLineAlignment(StringAlignmentCenter);
        SolidBrush textBr(Color(255, 248, 250, 252));
        RectF lineRc(rx, ry + 4.0f, rw, 18.0f);
        g.DrawString(tipText, -1, &tipFont, lineRc, &sf, &textBr);
    }
    } // Graphics g(memDC) scope ends and flushes before BitBlt to screen

    BitBlt(hdc, 0, 0, W, H, memDC, 0, 0, SRCCOPY);
    if (tempBackBuffer) {
        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);
    }
}

static void ExecuteOverlayDockButtonAction(HWND hWnd, const DockButton& b) {
    g_Daemon.CommitActiveTextBox();
    g_Daemon.CommitActiveSizeInput();
    g_Daemon.CommitActiveRatioInput();
    g_Daemon.CommitActiveStrokeInput();
    if (b.isTool) {
        g_Daemon.activeTool = b.tool;
        if (IsWindow(hWnd)) InvalidateRect(hWnd, nullptr, FALSE);
        return;
    }
    if (b.isColor) {
        g_Daemon.activeColor = b.swatchColor;
        g_Daemon.SaveSettings();
        if (g_Daemon.HasSelectedAnnotations()) {
            bool pushed = false;
            for (auto& a : g_Daemon.annotations) {
                if (g_Daemon.IsAnnotationSelected(a.id)) {
                    if (!pushed && a.color.GetValue() != b.swatchColor.GetValue()) {
                        g_Daemon.PushUndo();
                        pushed = true;
                    }
                    a.color = b.swatchColor;
                }
            }
        }
        if (g_Daemon.isEditingText) {
            g_Daemon.editingTextAnn.color = b.swatchColor;
        }
        if (IsWindow(hWnd)) InvalidateRect(hWnd, nullptr, FALSE);
        return;
    }
    if (b.isStroke) {
        g_Daemon.ApplyStrokeToSelectedAnnotation(b.strokeVal, true);
        g_Daemon.SaveSettings();
        if (IsWindow(hWnd)) InvalidateRect(hWnd, nullptr, FALSE);
        return;
    }
    switch (b.id) {
        case DBTN_ACT_SEARCH_GOOGLE: {
            // Shift turns the search into a translate: same clipboard handoff,
            // different page. See OpenGoogleTranslateInBrowser.
            bool shiftHeld = (GetKeyState(VK_SHIFT) & 0x8000) != 0 || (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            g_Daemon.ActionCopyAndClose(shiftHeld ? WebOpenTarget::Translate : WebOpenTarget::Lens);
            return;
        }
        case DBTN_ACT_COPY:
            // Shift does nothing here. The browser handoff belongs to the Search
            // on web button (and its Shift + click translate) alone; the pinned
            // toolbar's Copy is the one place Shift still changes what is copied.
            g_Daemon.ActionCopyAndClose(WebOpenTarget::None);
            return;
        case DBTN_ACT_SAVE:      g_Daemon.ActionQuickSaveAndClose(); return;
        case DBTN_ACT_SAVE_AS:   g_Daemon.ActionSaveAsAndClose(); return;
        case DBTN_ACT_PIN:       g_Daemon.ActionPinToDesktop(); return;
        case DBTN_ACT_OCR:       g_Daemon.ActionOcrAndClose(); return;
        case DBTN_ACT_UNDO:      g_Daemon.Undo(); return;
        case DBTN_ACT_REDO:      g_Daemon.Redo(); return;
        case DBTN_ACT_RESET_NUM:
            g_Daemon.nextStepNum = 1;
            if (IsWindow(hWnd)) InvalidateRect(hWnd, nullptr, FALSE);
            return;
        case DBTN_ACT_CLEAR:
            g_Daemon.PushUndo();
            g_Daemon.annotations.clear();
            g_Daemon.nextStepNum = 1;
            g_Daemon.ClearAnnotationSelection();
            if (IsWindow(hWnd)) InvalidateRect(hWnd, nullptr, FALSE);
            return;
        case DBTN_ACT_CLOSE:
            // Closing is not one call's work either: the settings write and the
            // surface-cache frees sit behind the window going away (see
            // HideOverlayForPendingWork), so the window goes first here too.
            g_Daemon.HideOverlayForPendingWork();
            g_Daemon.CloseRegionSnipOverlay();
            return;
    }
}

LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            SetTimer(hWnd, 1, 450, nullptr); // Caret blink timer for in-place text box
            SetTimer(hWnd, 2, 40, nullptr);  // Ctrl-key state poll for automatic window hover selection
            SetFocus(hWnd);
            return 0;

        case WM_WINDOWPOSCHANGING: {
            if (g_Daemon.IsEditingOpenedImage() &&
                g_Daemon.pinnedImageAboveEdit &&
                !g_Daemon.isSavingAsModalOpen &&
                (!g_Daemon.hOptionsWnd || !IsWindow(g_Daemon.hOptionsWnd)) &&
                (!g_Daemon.hCustomizeKeysWnd || !IsWindow(g_Daemon.hCustomizeKeysWnd)) &&
                (!g_Daemon.hNamingSyntaxWnd || !IsWindow(g_Daemon.hNamingSyntaxWnd)) &&
                (!g_Daemon.hShortcutsWnd || !IsWindow(g_Daemon.hShortcutsWnd))) {
                WINDOWPOS* wp = (WINDOWPOS*)lParam;
                if (wp && (wp->flags & SWP_NOZORDER) == 0) {
                    HWND hLowestPin = g_Daemon.GetLowestPinnedWindow();
                    if (hLowestPin && hLowestPin != hWnd) {
                        if (wp->hwndInsertAfter == HWND_TOP || wp->hwndInsertAfter == HWND_TOPMOST) {
                            wp->hwndInsertAfter = hLowestPin;
                        }
                    }
                }
            }
            break;
        }

        case WM_ACTIVATE:
            if (LOWORD(wParam) != WA_INACTIVE) {
                SetFocus(hWnd);
                if (g_Daemon.IsEditingOpenedImage() && g_Daemon.pinnedImageAboveEdit) {
                    g_Daemon.RaisePinnedWindowsAboveOverlay();
                }
            }
            return 0;

        case WM_TIMER:
            if (wParam == 1) {
                if (g_Daemon.isEditingText || g_Daemon.isEditingSize || g_Daemon.isEditingRatio || g_Daemon.isEditingStroke) {
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
            } else if (wParam == 2) {
                if (g_Daemon.isSavingAsModalOpen) {
                    return 0;
                }
                g_Daemon.SuppressAboveOverlayWindows();
                if ((!g_Daemon.hOptionsWnd || !IsWindow(g_Daemon.hOptionsWnd)) &&
                    (!g_Daemon.hCustomizeKeysWnd || !IsWindow(g_Daemon.hCustomizeKeysWnd)) &&
                    (!g_Daemon.hNamingSyntaxWnd || !IsWindow(g_Daemon.hNamingSyntaxWnd)) &&
                    (!g_Daemon.hShortcutsWnd || !IsWindow(g_Daemon.hShortcutsWnd))) {
                    if (g_Daemon.IsEditingOpenedImage() && g_Daemon.pinnedImageAboveEdit) {
                        HWND hLowestPin = g_Daemon.GetLowestPinnedWindow();
                        if (hLowestPin && hLowestPin != hWnd) {
                            SetWindowPos(hWnd, hLowestPin, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
                            g_Daemon.RaisePinnedWindowsAboveOverlay();
                        } else {
                            SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
                        }
                        HWND hFg = GetForegroundWindow();
                        if (hFg != hWnd && !g_Daemon.IsPinnedWindow(hFg)) {
                            LockSetForegroundWindow(LSFW_UNLOCK);
                            SetForegroundWindow(hWnd);
                            SetActiveWindow(hWnd);
                            SetFocus(hWnd);
                        }
                    } else {
                        SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
                        if (GetForegroundWindow() != hWnd) {
                            LockSetForegroundWindow(LSFW_UNLOCK);
                            SetForegroundWindow(hWnd);
                            SetActiveWindow(hWnd);
                            SetFocus(hWnd);
                        }
                    }
                }
                if (!g_Daemon.hasSelection &&
                    (g_Daemon.dragMode == DragMode::None ||
                     (g_Daemon.dragMode == DragMode::PendingOutsideSelection && (g_Daemon.pendingShiftColorPick || g_Daemon.pendingCtrlWindowSelect)))) {
                    g_Daemon.lastCtrlTempSelect = false;
                    bool curShiftColorPick = ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) ||
                                             ((GetKeyState(VK_SHIFT) & 0x8000) != 0) ||
                                             ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0) ||
                                             ((GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0);
                    bool shiftChanged = (curShiftColorPick != g_Daemon.lastShiftColorPick);
                    g_Daemon.lastShiftColorPick = curShiftColorPick;
                    if (g_Daemon.UpdateCtrlWindowHover() || shiftChanged) {
                        g_Daemon.UpdateOverlayCursor(g_Daemon.mousePt.x, g_Daemon.mousePt.y);
                        InvalidateRect(hWnd, nullptr, FALSE);
                    }
                } else if (g_Daemon.HasCreatedCustomArea()) {
                    bool curCtrlTempSelect = g_Daemon.IsCtrlTempSelectActive();
                    if (curCtrlTempSelect != g_Daemon.lastCtrlTempSelect) {
                        g_Daemon.lastCtrlTempSelect = curCtrlTempSelect;
                        g_Daemon.UpdateOverlayCursor(g_Daemon.mousePt.x, g_Daemon.mousePt.y);
                        InvalidateRect(hWnd, nullptr, FALSE);
                    }
                }
            } else if (wParam == 3) {
                KillTimer(hWnd, 3);
                if (g_Daemon.HasOverlayMultiFrames() && g_Daemon.overlayFramesPlaying) {
                    size_t nextIdx = (g_Daemon.overlayActiveFrameIdx + 1) % g_Daemon.overlayFrames.size();
                    g_Daemon.SelectOverlayFrame(nextIdx);
                    if (!g_Daemon.overlayFrameStripCollapsed) {
                        g_Daemon.EnsureActiveFrameVisibleInStrip();
                        g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                    }
                    int delayMs = (nextIdx < g_Daemon.overlayFrameDelaysMs.size()) ? g_Daemon.overlayFrameDelaysMs[nextIdx] : 80;
                    delayMs = std::max(20, std::min(60000, delayMs));
                    SetTimer(hWnd, 3, (UINT)delayMs, nullptr);
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
            } else if (wParam == 4) {
                KillTimer(hWnd, 4);
                if ((!g_Daemon.hOptionsWnd || !IsWindow(g_Daemon.hOptionsWnd)) &&
                    (!g_Daemon.hCustomizeKeysWnd || !IsWindow(g_Daemon.hCustomizeKeysWnd)) &&
                    (!g_Daemon.hNamingSyntaxWnd || !IsWindow(g_Daemon.hNamingSyntaxWnd)) &&
                    (!g_Daemon.hShortcutsWnd || !IsWindow(g_Daemon.hShortcutsWnd)) &&
                    !g_Daemon.isSavingAsModalOpen) {
                    POINT ptScreen;
                    if (GetCursorPos(&ptScreen)) {
                        HWND hHit = WindowFromPoint(ptScreen);
                        if (!hHit || !g_Daemon.IsPinnedWindow(hHit)) {
                            POINT ptOverlay = { ptScreen.x - g_Daemon.vScreenX, ptScreen.y - g_Daemon.vScreenY };
                            if (g_Daemon.IsHoveringSelectedCustomArea(ptOverlay)) {
                                g_Daemon.showingCustomAreaHoverTip = true;
                                g_Daemon.customAreaHoverTipPt = ptOverlay;
                                InvalidateRect(hWnd, nullptr, FALSE);
                            }
                        }
                    }
                }
            }
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT) {
                g_Daemon.UpdateOverlayCursor(g_Daemon.mousePt.x, g_Daemon.mousePt.y);
                return TRUE;
            }
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RenderOverlayWindow(hWnd, hdc);
            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_RBUTTONDOWN: {
            KillTimer(hWnd, 4);
            if (g_Daemon.showingCustomAreaHoverTip) {
                g_Daemon.showingCustomAreaHoverTip = false;
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            SetFocus(hWnd);
            int mx = GET_X_LPARAM(lParam);
            int my = GET_Y_LPARAM(lParam);
            g_Daemon.mousePt = { mx, my };
            g_Daemon.isRightMouseHeld = true;
            bool isSelectionOrResizeDrag =
                (g_Daemon.dragMode == DragMode::PendingOutsideSelection ||
                 g_Daemon.dragMode == DragMode::CreatingSelection ||
                 (g_Daemon.dragMode >= DragMode::ResizeTL && g_Daemon.dragMode <= DragMode::ResizeL));
            if (isSelectionOrResizeDrag) {
                WPARAM moveWParam = (wParam | MK_RBUTTON);
                if ((GetKeyState(VK_LBUTTON) & 0x8000) != 0) moveWParam |= MK_LBUTTON;
                SendMessageW(hWnd, WM_MOUSEMOVE, moveWParam, MAKELPARAM(mx, my));
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_RBUTTONUP: {
            int mx = GET_X_LPARAM(lParam);
            int my = GET_Y_LPARAM(lParam);
            g_Daemon.mousePt = { mx, my };
            g_Daemon.isRightMouseHeld = false;
            bool isSelectionOrResizeDrag =
                (g_Daemon.dragMode == DragMode::PendingOutsideSelection ||
                 g_Daemon.dragMode == DragMode::CreatingSelection ||
                 (g_Daemon.dragMode >= DragMode::ResizeTL && g_Daemon.dragMode <= DragMode::ResizeL));
            if (isSelectionOrResizeDrag) {
                WPARAM moveWParam = (wParam & ~MK_RBUTTON);
                if ((GetKeyState(VK_LBUTTON) & 0x8000) != 0) moveWParam |= MK_LBUTTON;
                SendMessageW(hWnd, WM_MOUSEMOVE, moveWParam, MAKELPARAM(mx, my));
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            KillTimer(hWnd, 4);
            g_Daemon.showingCustomAreaHoverTip = false;
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            if (g_Daemon.HasOverlayMultiFrames() && !g_Daemon.overlayFrameStripCollapsed &&
                PtInRect(&g_Daemon.overlayFrameStripPanelRect, g_Daemon.mousePt)) {
                float step = (delta > 0) ? -108.0f : 108.0f;
                g_Daemon.overlayFrameScrollX = std::max(0.0f, std::min(g_Daemon.overlayFrameMaxScrollX, g_Daemon.overlayFrameScrollX + step));
                g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            bool shiftHeld = ((GET_KEYSTATE_WPARAM(wParam) & MK_SHIFT) != 0) ||
                             ((GetKeyState(VK_SHIFT) & 0x8000) != 0) ||
                             ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) ||
                             ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0) ||
                             ((GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0);
            if (!g_Daemon.disableMagnifierWindow && !g_Daemon.hasSelection && shiftHeld && delta != 0) {
                int mag = g_Daemon.loupeMagnification;
                if (delta > 0) {
                    if (mag < 10) mag += 1;
                    else if (mag < 20) mag += 2;
                    else if (mag < 50) mag += 5;
                    else if (mag < 90) mag += 10;
                    else mag += 30;
                    mag = std::min(210, mag);
                } else {
                    if (mag > 90) mag -= 30;
                    else if (mag > 50) mag -= 10;
                    else if (mag > 20) mag -= 5;
                    else if (mag > 10) mag -= 2;
                    else mag -= 1;
                    mag = std::max(1, mag);
                }
                if (mag != g_Daemon.loupeMagnification) {
                    g_Daemon.loupeMagnification = mag;
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                return 0;
            }
            if (!g_Daemon.hasSelection) {
                return 0;
            }
            float nextS = std::max(1.0f, std::min(120.0f, g_Daemon.activeStroke + (delta > 0 ? 1.5f : -1.5f)));
            g_Daemon.ApplyStrokeToSelectedAnnotation(nextS, false);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        case WM_MOUSEHWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            if (g_Daemon.HasOverlayMultiFrames() && !g_Daemon.overlayFrameStripCollapsed &&
                PtInRect(&g_Daemon.overlayFrameStripPanelRect, g_Daemon.mousePt)) {
                float step = (delta > 0) ? 108.0f : -108.0f;
                g_Daemon.overlayFrameScrollX = std::max(0.0f, std::min(g_Daemon.overlayFrameMaxScrollX, g_Daemon.overlayFrameScrollX + step));
                g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            break;
        }

        case WM_LBUTTONDBLCLK: {
            KillTimer(hWnd, 4);
            g_Daemon.showingCustomAreaHoverTip = false;
            int mx = GET_X_LPARAM(lParam);
            int my = GET_Y_LPARAM(lParam);
            POINT pt{ mx, my };
            if (g_Daemon.HasOverlayMultiFrames()) {
                g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                if (PtInRect(&g_Daemon.overlayFrameStripToggleBtnRect, pt)) {
                    g_Daemon.overlayFramePressedItem = PtInRect(&g_Daemon.overlayFrameStripPlayBtnRect, pt)
                        ? PepperSnapDaemon::FrameStripPressedItem::PlayPause
                        : PepperSnapDaemon::FrameStripPressedItem::ToggleCollapse;
                    SetCapture(hWnd);
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
                if (!g_Daemon.overlayFrameStripCollapsed && PtInRect(&g_Daemon.overlayFrameStripPanelRect, pt)) {
                    if (PtInRect(&g_Daemon.overlayFrameStripScrollLeftRect, pt)) {
                        g_Daemon.overlayFramePressedItem = PepperSnapDaemon::FrameStripPressedItem::ScrollLeft;
                        SetCapture(hWnd);
                        InvalidateRect(hWnd, nullptr, FALSE);
                        return 0;
                    }
                    if (PtInRect(&g_Daemon.overlayFrameStripScrollRightRect, pt)) {
                        g_Daemon.overlayFramePressedItem = PepperSnapDaemon::FrameStripPressedItem::ScrollRight;
                        SetCapture(hWnd);
                        InvalidateRect(hWnd, nullptr, FALSE);
                        return 0;
                    }
                    for (size_t i = 0; i < g_Daemon.overlayFrameCardRects.size(); ++i) {
                        if (PtInRect(&g_Daemon.overlayFrameCardRects[i], pt)) {
                            g_Daemon.overlayFramePressedItem = PepperSnapDaemon::FrameStripPressedItem::FrameCard;
                            g_Daemon.overlayFramePressedCardIdx = (int)i;
                            SetCapture(hWnd);
                            InvalidateRect(hWnd, nullptr, FALSE);
                            return 0;
                        }
                    }
                    return 0;
                }
            }
            for (const auto& b : g_Daemon.dockButtons) {
                if (PtInRect(&b.rect, pt) && b.id != DBTN_ACT_DRAG_HUD) {
                    g_Daemon.pressedBtnId = b.id;
                    g_Daemon.hoveredBtnId = b.id;
                    SetCapture(hWnd);
                    InvalidateRect(hWnd, nullptr, FALSE);
                    UpdateWindow(hWnd); // Show the pressed state at once, not at the next idle moment
                    return 0;
                }
            }
            if (g_Daemon.hasSelection && PtInRect(&g_Daemon.customStrokeRect, pt)) {
                if (!g_Daemon.isEditingStroke) {
                    g_Daemon.isEditingStroke = true;
                    g_Daemon.editingStrokeText = std::to_wstring((int)std::round(g_Daemon.activeStroke));
                }
                g_Daemon.strokeSelAnchor = 0;
                g_Daemon.strokeCaretPos = g_Daemon.editingStrokeText.size();
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (g_Daemon.hasSelection && g_Daemon.hasCustomRatio && PtInRect(&g_Daemon.ratioResetBtnRect, pt)) {
                g_Daemon.isEditingRatio = false;
                g_Daemon.isDraggingRatioText = false;
                g_Daemon.editingRatioText.clear();
                g_Daemon.ratioCaretPos = g_Daemon.ratioSelAnchor = 0;
                g_Daemon.hasCustomRatio = false;
                g_Daemon.customRatioW = 0;
                g_Daemon.customRatioH = 0;
                g_Daemon.activeSelectionRatioW = 0;
                g_Daemon.activeSelectionRatioH = 0;
                g_Daemon.ratioResetBtnRect = { 0, 0, 0, 0 };
                g_Daemon.lastRatioResetHover = false;
                g_Daemon.UpdateOverlayCursor(mx, my);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (g_Daemon.hasSelection && PtInRect(&g_Daemon.dimPillRect, pt)) {
                if (!g_Daemon.isEditingSize) {
                    int sw = std::abs(g_Daemon.selRect.right - g_Daemon.selRect.left);
                    int sh = std::abs(g_Daemon.selRect.bottom - g_Daemon.selRect.top);
                    g_Daemon.isEditingSize = true;
                    g_Daemon.editingSizeText = std::to_wstring(sw) + L"x" + std::to_wstring(sh);
                }
                size_t idx = PepperSnapDaemon::HitTestMonoIndex(mx, g_Daemon.dimPillRect, g_Daemon.editingSizeText, L" px");
                const std::wstring& s = g_Daemon.editingSizeText;
                if (idx < s.size() && (s[idx] >= L'0' && s[idx] <= L'9')) {
                    size_t l = idx, r = idx;
                    while (l > 0 && s[l - 1] >= L'0' && s[l - 1] <= L'9') l--;
                    while (r < s.size() && s[r] >= L'0' && s[r] <= L'9') r++;
                    g_Daemon.sizeSelAnchor = l;
                    g_Daemon.sizeCaretPos = r;
                } else {
                    g_Daemon.sizeSelAnchor = 0;
                    g_Daemon.sizeCaretPos = s.size();
                }
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (g_Daemon.hasSelection && PtInRect(&g_Daemon.ratioPillRect, pt)) {
                if (!g_Daemon.isEditingRatio) {
                    int sw = std::max(1, (int)std::abs(g_Daemon.selRect.right - g_Daemon.selRect.left));
                    int sh = std::max(1, (int)std::abs(g_Daemon.selRect.bottom - g_Daemon.selRect.top));
                    int gVal = PepperSnapDaemon::ComputeIntGcd(sw, sh);
                    int rw = (g_Daemon.hasCustomRatio && g_Daemon.customRatioW > 0) ? g_Daemon.customRatioW : (sw / gVal);
                    int rh = (g_Daemon.hasCustomRatio && g_Daemon.customRatioH > 0) ? g_Daemon.customRatioH : (sh / gVal);
                    g_Daemon.isEditingRatio = true;
                    g_Daemon.editingRatioText = std::to_wstring(rw) + L":" + std::to_wstring(rh);
                }
                size_t idx = PepperSnapDaemon::HitTestMonoIndex(mx, g_Daemon.ratioPillRect, g_Daemon.editingRatioText, L"");
                const std::wstring& s = g_Daemon.editingRatioText;
                if (idx < s.size() && (s[idx] >= L'0' && s[idx] <= L'9')) {
                    size_t l = idx, r = idx;
                    while (l > 0 && s[l - 1] >= L'0' && s[l - 1] <= L'9') l--;
                    while (r < s.size() && s[r] >= L'0' && s[r] <= L'9') r++;
                    g_Daemon.ratioSelAnchor = l;
                    g_Daemon.ratioCaretPos = r;
                } else {
                    g_Daemon.ratioSelAnchor = 0;
                    g_Daemon.ratioCaretPos = s.size();
                }
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (g_Daemon.isEditingText) {
                RECT tbRc = g_Daemon.GetEditingTextBoxRect();
                if (PtInRect(&tbRc, pt)) {
                    size_t idx = g_Daemon.HitTestTextBoxIndex(mx, my);
                    const std::wstring& s = g_Daemon.editingTextAnn.text;
                    if (idx < s.size() && s[idx] != L' ' && s[idx] != L'\n') {
                        size_t l = idx, r = idx;
                        while (l > 0 && s[l - 1] != L' ' && s[l - 1] != L'\n') l--;
                        while (r < s.size() && s[r] != L' ' && s[r] != L'\n') r++;
                        g_Daemon.textSelAnchor = l;
                        g_Daemon.textCaretPos = r;
                    } else {
                        g_Daemon.textSelAnchor = 0;
                        g_Daemon.textCaretPos = s.size();
                    }
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            KillTimer(hWnd, 4);
            g_Daemon.showingCustomAreaHoverTip = false;
            if (GetForegroundWindow() != hWnd) {
                SetForegroundWindow(hWnd);
                SetActiveWindow(hWnd);
            }
            SetFocus(hWnd);
            if (g_Daemon.IsEditingOpenedImage()) {
                g_Daemon.RaisePinnedWindowsAboveOverlay();
            }
            int mx = GET_X_LPARAM(lParam);
            int my = GET_Y_LPARAM(lParam);
            POINT pt{ mx, my };
            g_Daemon.mousePt = pt;
            bool shiftHeld = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

            // -1. Multi-Frame GIF / WebP Selector Strip (on top of custom area and its toolbar)
            if (g_Daemon.HasOverlayMultiFrames()) {
                g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                if (PtInRect(&g_Daemon.overlayFrameStripToggleBtnRect, pt)) {
                    g_Daemon.CommitActiveTextBox();
                    g_Daemon.CommitActiveSizeInput();
                    g_Daemon.CommitActiveRatioInput();
                    g_Daemon.CommitActiveStrokeInput();
                    g_Daemon.overlayFramePressedItem = PtInRect(&g_Daemon.overlayFrameStripPlayBtnRect, pt)
                        ? PepperSnapDaemon::FrameStripPressedItem::PlayPause
                        : PepperSnapDaemon::FrameStripPressedItem::ToggleCollapse;
                    SetCapture(hWnd);
                    g_Daemon.UpdateOverlayCursor(mx, my);
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
                if (!g_Daemon.overlayFrameStripCollapsed) {
                    if (PtInRect(&g_Daemon.overlayFrameStripResizeRect, pt)) {
                        g_Daemon.CommitActiveTextBox();
                        g_Daemon.CommitActiveSizeInput();
                        g_Daemon.CommitActiveRatioInput();
                        g_Daemon.CommitActiveStrokeInput();
                        g_Daemon.dragMode = DragMode::ResizingFrameStrip;
                        g_Daemon.dragFrameStripStartY = my;
                        g_Daemon.dragFrameStripOrigH = g_Daemon.overlayFrameStripHeight;
                        SetCapture(hWnd);
                        g_Daemon.UpdateOverlayCursor(mx, my);
                        InvalidateRect(hWnd, nullptr, FALSE);
                        return 0;
                    }
                    if (PtInRect(&g_Daemon.overlayFrameStripPanelRect, pt)) {
                        g_Daemon.CommitActiveTextBox();
                        g_Daemon.CommitActiveSizeInput();
                        g_Daemon.CommitActiveRatioInput();
                        g_Daemon.CommitActiveStrokeInput();
                        if (PtInRect(&g_Daemon.overlayFrameStripScrollLeftRect, pt)) {
                            g_Daemon.overlayFramePressedItem = PepperSnapDaemon::FrameStripPressedItem::ScrollLeft;
                            SetCapture(hWnd);
                            InvalidateRect(hWnd, nullptr, FALSE);
                            return 0;
                        }
                        if (PtInRect(&g_Daemon.overlayFrameStripScrollRightRect, pt)) {
                            g_Daemon.overlayFramePressedItem = PepperSnapDaemon::FrameStripPressedItem::ScrollRight;
                            SetCapture(hWnd);
                            InvalidateRect(hWnd, nullptr, FALSE);
                            return 0;
                        }
                        if (PtInRect(&g_Daemon.overlayFrameStripScrollThumbRect, pt)) {
                            g_Daemon.dragMode = DragMode::ScrollingFrameStrip;
                            g_Daemon.dragFrameScrollStartX = mx;
                            g_Daemon.dragFrameScrollOrigX = g_Daemon.overlayFrameScrollX;
                            SetCapture(hWnd);
                            g_Daemon.UpdateOverlayCursor(mx, my);
                            InvalidateRect(hWnd, nullptr, FALSE);
                            return 0;
                        }
                        if (PtInRect(&g_Daemon.overlayFrameStripScrollTrackRect, pt)) {
                            int trackW = std::max(1, (int)(g_Daemon.overlayFrameStripScrollTrackRect.right - g_Daemon.overlayFrameStripScrollTrackRect.left));
                            int thumbW = std::max(24, (int)(g_Daemon.overlayFrameStripScrollThumbRect.right - g_Daemon.overlayFrameStripScrollThumbRect.left));
                            float ratio = (float)(mx - g_Daemon.overlayFrameStripScrollTrackRect.left - thumbW / 2) / (float)std::max(1, trackW - thumbW);
                            ratio = std::max(0.0f, std::min(1.0f, ratio));
                            g_Daemon.overlayFrameScrollX = ratio * g_Daemon.overlayFrameMaxScrollX;
                            g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                            g_Daemon.dragMode = DragMode::ScrollingFrameStrip;
                            g_Daemon.dragFrameScrollStartX = mx;
                            g_Daemon.dragFrameScrollOrigX = g_Daemon.overlayFrameScrollX;
                            SetCapture(hWnd);
                            InvalidateRect(hWnd, nullptr, FALSE);
                            return 0;
                        }
                        for (size_t i = 0; i < g_Daemon.overlayFrameCardRects.size(); ++i) {
                            if (PtInRect(&g_Daemon.overlayFrameCardRects[i], pt)) {
                                g_Daemon.overlayFramePressedItem = PepperSnapDaemon::FrameStripPressedItem::FrameCard;
                                g_Daemon.overlayFramePressedCardIdx = (int)i;
                                SetCapture(hWnd);
                                InvalidateRect(hWnd, nullptr, FALSE);
                                return 0;
                            }
                        }
                        return 0;
                    }
                }
            }

            // Auto-hide the frame list whenever the user focuses on the custom area or its toolbar (when enabled in Options)
            auto autoHideFrameStripOnFocus = [&]() {
                if (g_Daemon.autoHideFrameList && g_Daemon.HasOverlayMultiFrames() && !g_Daemon.overlayFrameStripCollapsed) {
                    g_Daemon.overlayFrameStripCollapsed = true;
                    g_Daemon.overlayHoveredFrameIdx = -1;
                    g_Daemon.overlayTopResizeHovered = false;
                    g_Daemon.overlayScrollbarHovered = false;
                    g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                }
            };

            // Check Typeable Custom Stroke / Text Size Input Box (Right of [XL])
            if (g_Daemon.hasSelection && PtInRect(&g_Daemon.customStrokeRect, pt)) {
                autoHideFrameStripOnFocus();
                g_Daemon.CommitActiveTextBox();
                g_Daemon.CommitActiveSizeInput();
                g_Daemon.CommitActiveRatioInput();
                if (!g_Daemon.isEditingStroke) {
                    g_Daemon.isEditingStroke = true;
                    g_Daemon.editingStrokeText = std::to_wstring((int)std::round(g_Daemon.activeStroke));
                }
                size_t idx = PepperSnapDaemon::HitTestMonoIndex(mx, g_Daemon.customStrokeRect, g_Daemon.editingStrokeText, L" px");
                g_Daemon.strokeCaretPos = idx;
                if (!shiftHeld) g_Daemon.strokeSelAnchor = idx;
                g_Daemon.isDraggingStrokeText = true;
                SetCapture(hWnd);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (g_Daemon.isEditingStroke) {
                g_Daemon.CommitActiveStrokeInput();
                InvalidateRect(hWnd, nullptr, FALSE);
            }

            // Check Typeable Size & Ratio Indicator Pill Click (unless grabbing a resize handle while not editing size/ratio)
            DragMode preHandleHit = (g_Daemon.isEditingSize || g_Daemon.isEditingRatio) ? DragMode::None : g_Daemon.HitTestSelectionHandles(mx, my);
            if (preHandleHit == DragMode::None && g_Daemon.hasSelection && g_Daemon.hasCustomRatio && PtInRect(&g_Daemon.ratioResetBtnRect, pt)) {
                autoHideFrameStripOnFocus();
                g_Daemon.CommitActiveTextBox();
                g_Daemon.CommitActiveSizeInput();
                g_Daemon.isEditingRatio = false;
                g_Daemon.isDraggingRatioText = false;
                g_Daemon.editingRatioText.clear();
                g_Daemon.ratioCaretPos = g_Daemon.ratioSelAnchor = 0;
                g_Daemon.CommitActiveStrokeInput();
                g_Daemon.hasCustomRatio = false;
                g_Daemon.customRatioW = 0;
                g_Daemon.customRatioH = 0;
                g_Daemon.activeSelectionRatioW = 0;
                g_Daemon.activeSelectionRatioH = 0;
                g_Daemon.ratioResetBtnRect = { 0, 0, 0, 0 };
                g_Daemon.lastRatioResetHover = false;
                g_Daemon.UpdateOverlayCursor(mx, my);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (preHandleHit == DragMode::None && g_Daemon.hasSelection && PtInRect(&g_Daemon.dimPillRect, pt)) {
                autoHideFrameStripOnFocus();
                g_Daemon.CommitActiveTextBox();
                g_Daemon.CommitActiveRatioInput();
                if (!g_Daemon.isEditingSize) {
                    int sw = std::abs(g_Daemon.selRect.right - g_Daemon.selRect.left);
                    int sh = std::abs(g_Daemon.selRect.bottom - g_Daemon.selRect.top);
                    g_Daemon.isEditingSize = true;
                    g_Daemon.editingSizeText = std::to_wstring(sw) + L"x" + std::to_wstring(sh);
                }
                size_t idx = PepperSnapDaemon::HitTestMonoIndex(mx, g_Daemon.dimPillRect, g_Daemon.editingSizeText, L" px");
                g_Daemon.sizeCaretPos = idx;
                if (!shiftHeld) g_Daemon.sizeSelAnchor = idx;
                g_Daemon.isDraggingSizeText = true;
                SetCapture(hWnd);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (g_Daemon.isEditingSize) {
                g_Daemon.CommitActiveSizeInput();
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            if (preHandleHit == DragMode::None && g_Daemon.hasSelection && PtInRect(&g_Daemon.ratioPillRect, pt)) {
                autoHideFrameStripOnFocus();
                g_Daemon.CommitActiveTextBox();
                g_Daemon.CommitActiveSizeInput();
                g_Daemon.CommitActiveStrokeInput();
                if (!g_Daemon.isEditingRatio) {
                    int sw = std::max(1, (int)std::abs(g_Daemon.selRect.right - g_Daemon.selRect.left));
                    int sh = std::max(1, (int)std::abs(g_Daemon.selRect.bottom - g_Daemon.selRect.top));
                    int gVal = PepperSnapDaemon::ComputeIntGcd(sw, sh);
                    int rw = (g_Daemon.hasCustomRatio && g_Daemon.customRatioW > 0) ? g_Daemon.customRatioW : (sw / gVal);
                    int rh = (g_Daemon.hasCustomRatio && g_Daemon.customRatioH > 0) ? g_Daemon.customRatioH : (sh / gVal);
                    g_Daemon.isEditingRatio = true;
                    g_Daemon.editingRatioText = std::to_wstring(rw) + L":" + std::to_wstring(rh);
                }
                size_t idx = PepperSnapDaemon::HitTestMonoIndex(mx, g_Daemon.ratioPillRect, g_Daemon.editingRatioText, L"");
                g_Daemon.ratioCaretPos = idx;
                if (!shiftHeld) g_Daemon.ratioSelAnchor = idx;
                g_Daemon.isDraggingRatioText = true;
                SetCapture(hWnd);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (g_Daemon.isEditingRatio) {
                g_Daemon.CommitActiveRatioInput();
                InvalidateRect(hWnd, nullptr, FALSE);
            }

            // Check Docked Toolbar Buttons
            for (const auto& b : g_Daemon.dockButtons) {
                if (PtInRect(&b.rect, pt)) {
                    autoHideFrameStripOnFocus();
                    if (b.id == DBTN_ACT_DRAG_HUD) {
                        g_Daemon.CommitActiveTextBox();
                        if (!g_Daemon.hasCustomHudPos) {
                            g_Daemon.customHudPos = { g_Daemon.hudBoundsRect.right, g_Daemon.hudBoundsRect.top };
                            g_Daemon.hasCustomHudPos = true;
                        }
                        g_Daemon.hoveredBtnId = -1;
                        g_Daemon.pressedBtnId = -1;
                        g_Daemon.dragMode = DragMode::DraggingHUD;
                        g_Daemon.dragHudStartMouse = pt;
                        g_Daemon.dragHudOrigPos = g_Daemon.customHudPos;
                        SetCapture(hWnd);
                        g_Daemon.UpdateOverlayCursor(mx, my);
                        InvalidateRect(hWnd, nullptr, FALSE);
                        return 0;
                    }
                    g_Daemon.pressedBtnId = b.id;
                    g_Daemon.hoveredBtnId = b.id;
                    SetCapture(hWnd);
                    InvalidateRect(hWnd, nullptr, FALSE);
                    UpdateWindow(hWnd); // Show the pressed state at once, not at the next idle moment
                    return 0;
                }
            }

            // If currently editing a text box, check if clicking inside it to move caret / drag-select!
            if (g_Daemon.isEditingText) {
                RECT tbRc = g_Daemon.GetEditingTextBoxRect();
                if (PtInRect(&tbRc, pt)) {
                    autoHideFrameStripOnFocus();
                    size_t idx = g_Daemon.HitTestTextBoxIndex(mx, my);
                    g_Daemon.textCaretPos = idx;
                    if (!shiftHeld) g_Daemon.textSelAnchor = idx;
                    g_Daemon.isDraggingTextBoxSel = true;
                    SetCapture(hWnd);
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
                g_Daemon.CommitActiveTextBox();
                InvalidateRect(hWnd, nullptr, FALSE);
            }

            // Check 8 Selection Resize Handles (Ctrl + Left-Click on a corner handle moves that corner individually!)
            DragMode handleHit = g_Daemon.HitTestSelectionHandles(mx, my);
            if (handleHit != DragMode::None) {
                autoHideFrameStripOnFocus();
                g_Daemon.isRightMouseHeld = ((wParam & MK_RBUTTON) != 0) || ((GetKeyState(VK_RBUTTON) & 0x8000) != 0);
                bool ctrlHeldOnHandle = ((wParam & MK_CONTROL) != 0) ||
                                        ((GetKeyState(VK_CONTROL) & 0x8000) != 0) ||
                                        ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) ||
                                        ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0) ||
                                        ((GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0);
                bool isCornerHandle = (handleHit == DragMode::ResizeTL ||
                                       handleHit == DragMode::ResizeTR ||
                                       handleHit == DragMode::ResizeBR ||
                                       handleHit == DragMode::ResizeBL);
                g_Daemon.isDraggingSingleCorner = (isCornerHandle && ctrlHeldOnHandle && !g_Daemon.hasEllipseSelection);
                g_Daemon.dragMode = handleHit;
                g_Daemon.dragStartPt = pt;
                g_Daemon.GetSelectionQuadPoints(g_Daemon.dragOrigQuad);
                g_Daemon.dragOrigRect = {
                    std::min(g_Daemon.selRect.left, g_Daemon.selRect.right),
                    std::min(g_Daemon.selRect.top, g_Daemon.selRect.bottom),
                    std::max(g_Daemon.selRect.left, g_Daemon.selRect.right),
                    std::max(g_Daemon.selRect.top, g_Daemon.selRect.bottom)
                };
                g_Daemon.selectionTargetPt = pt;
                SetCapture(hWnd);
                SendMessageW(hWnd, WM_MOUSEMOVE, wParam | MK_LBUTTON, MAKELPARAM(mx, my));
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }

            // Inside Selection Box / Polygon
            if (g_Daemon.hasSelection) {
                RECT normSel = {
                    std::min(g_Daemon.selRect.left, g_Daemon.selRect.right),
                    std::min(g_Daemon.selRect.top, g_Daemon.selRect.bottom),
                    std::max(g_Daemon.selRect.left, g_Daemon.selRect.right),
                    std::max(g_Daemon.selRect.top, g_Daemon.selRect.bottom)
                };
                if (g_Daemon.IsPointInSelection(pt)) {
                    autoHideFrameStripOnFocus();
                    OverlayTool effectiveTool = g_Daemon.GetEffectiveTool();
                    if (effectiveTool == OverlayTool::SelectMove) {
                        bool multiMod = shiftHeld;
                        DragMode annDragMode = DragMode::MovingAnnotation;
                        int hitId = g_Daemon.HitTestAnnotation((float)mx, (float)my, &annDragMode);
                        g_Daemon.moveAnnUndoPushed = false;
                        g_Daemon.clickedAlreadySelectedAnn = false;
                        g_Daemon.clickedAlreadySelectedAnnId = -1;
                        if (hitId != -1) {
                            if (multiMod) {
                                g_Daemon.ToggleAnnotationSelection(hitId);
                                if (g_Daemon.IsAnnotationSelected(hitId)) {
                                    g_Daemon.dragMode = DragMode::MovingAnnotation;
                                    g_Daemon.dragStartPt = pt;
                                    for (const auto& a : g_Daemon.annotations) {
                                        if (a.id == hitId) {
                                            g_Daemon.activeStroke = a.strokeWidth;
                                            g_Daemon.activeColor = a.color;
                                            g_Daemon.moveAnnOffset = { (float)mx - a.startPt.x, (float)my - a.startPt.y };
                                            break;
                                        }
                                    }
                                } else {
                                    g_Daemon.dragMode = DragMode::None;
                                }
                            } else {
                                if (g_Daemon.IsAnnotationSelected(hitId) && g_Daemon.selectedAnnotationIds.size() > 1) {
                                    // Keep the multi-selection intact so dragging moves all selected annotations together;
                                    // if the user releases without dragging, select only this clicked annotation.
                                    g_Daemon.selectedAnnotationId = hitId;
                                    g_Daemon.clickedAlreadySelectedAnn = true;
                                    g_Daemon.clickedAlreadySelectedAnnId = hitId;
                                    g_Daemon.dragMode = DragMode::MovingAnnotation;
                                } else {
                                    g_Daemon.SelectSingleAnnotation(hitId);
                                    g_Daemon.dragMode = annDragMode;
                                }
                                g_Daemon.dragStartPt = pt;
                                for (const auto& a : g_Daemon.annotations) {
                                    if (a.id == hitId) {
                                        g_Daemon.activeStroke = a.strokeWidth;
                                        g_Daemon.activeColor = a.color;
                                        g_Daemon.moveAnnOffset = { (float)mx - a.startPt.x, (float)my - a.startPt.y };
                                        break;
                                    }
                                }
                            }
                        } else if (multiMod) {
                            // Shift + drag (or Ctrl + drag) on empty space inside custom area starts marquee multi-selection!
                            g_Daemon.dragMode = DragMode::MarqueeSelectingAnnotations;
                            g_Daemon.marqueeStartPt = pt;
                            g_Daemon.marqueeRect = { pt.x, pt.y, pt.x, pt.y };
                            g_Daemon.hasMarqueeBox = true;
                        } else {
                            g_Daemon.ClearAnnotationSelection();
                            g_Daemon.dragMode = DragMode::MovingSelection;
                            g_Daemon.dragStartPt = pt;
                            g_Daemon.dragOrigRect = normSel;
                            g_Daemon.GetSelectionQuadPoints(g_Daemon.dragOrigQuad);
                        }
                        SetCapture(hWnd);
                        InvalidateRect(hWnd, nullptr, FALSE);
                        return 0;
                    }

                    // Interactive In-Place Text Box Tool (Click existing text to re-edit, or click to create new)
                    if (g_Daemon.activeTool == OverlayTool::TextBox) {
                        for (auto it = g_Daemon.annotations.rbegin(); it != g_Daemon.annotations.rend(); ++it) {
                            if (it->tool == OverlayTool::TextBox) {
                                float minX = std::min(it->startPt.x, it->endPt.x) - 8.0f;
                                float maxX = std::max(it->startPt.x, it->endPt.x) + 8.0f;
                                float minY = std::min(it->startPt.y, it->endPt.y) - 8.0f;
                                float maxY = std::max(it->startPt.y, it->endPt.y) + 8.0f;
                                if (mx >= minX && mx <= maxX && my >= minY && my <= maxY) {
                                    g_Daemon.editingTextAnn = *it;
                                    g_Daemon.annotations.erase(std::next(it).base());
                                    g_Daemon.isEditingText = true;
                                    size_t idx = g_Daemon.HitTestTextBoxIndex(mx, my);
                                    g_Daemon.textCaretPos = idx;
                                    g_Daemon.textSelAnchor = idx;
                                    g_Daemon.isDraggingTextBoxSel = true;
                                    SetCapture(hWnd);
                                    InvalidateRect(hWnd, nullptr, FALSE);
                                    return 0;
                                }
                            }
                        }
                        g_Daemon.isEditingText = true;
                        g_Daemon.isDraggingTextBoxSel = false;
                        g_Daemon.textCaretPos = 0;
                        g_Daemon.textSelAnchor = 0;
                        g_Daemon.editingTextAnn = Annotation();
                        g_Daemon.editingTextAnn.id = g_Daemon.nextAnnotationId++;
                        g_Daemon.editingTextAnn.tool = OverlayTool::TextBox;
                        g_Daemon.editingTextAnn.color = g_Daemon.activeColor;
                        g_Daemon.editingTextAnn.strokeWidth = g_Daemon.activeStroke;
                        g_Daemon.editingTextAnn.startPt = { (float)mx, (float)my };
                        g_Daemon.editingTextAnn.endPt   = { (float)(mx + 160), (float)(my + 34) };
                        g_Daemon.editingTextAnn.text = L"";
                        InvalidateRect(hWnd, nullptr, FALSE);
                        return 0;
                    }

                    // Start drawing shape / Pen / Arrow / NumberArrow / Pixelate
                    g_Daemon.ClearAnnotationSelection();
                    g_Daemon.dragMode = DragMode::DrawingAnnotation;
                    g_Daemon.draftAnn = Annotation();
                    g_Daemon.draftAnn.id = g_Daemon.nextAnnotationId++;
                    g_Daemon.draftAnn.tool = g_Daemon.activeTool;
                    g_Daemon.draftAnn.color = g_Daemon.activeColor;
                    g_Daemon.draftAnn.strokeWidth = g_Daemon.activeStroke;
                    g_Daemon.draftAnn.startPt = { (float)mx, (float)my };
                    g_Daemon.draftAnn.endPt   = { (float)mx, (float)my };
                    g_Daemon.draftAnn.points.push_back({ (float)mx, (float)my });
                    g_Daemon.penSnapAnchor    = { (float)mx, (float)my };
                    g_Daemon.isPenShiftSnapping = false;
                    if (g_Daemon.activeTool == OverlayTool::NumberArrow) {
                        g_Daemon.draftAnn.stepNumber = g_Daemon.nextStepNum;
                    }
                    SetCapture(hWnd);
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
            }

            // Outside Selection Box -> Enter PendingOutsideSelection
            // (Shift + click picks pixel RGB color; Ctrl + click selects window on WM_LBUTTONUP release if not dragged;
            //  manual region creation starts in WM_MOUSEMOVE only if dragged >= 5px)
            g_Daemon.ClearAnnotationSelection();
            g_Daemon.isRightMouseHeld = ((wParam & MK_RBUTTON) != 0) || ((GetKeyState(VK_RBUTTON) & 0x8000) != 0);
            g_Daemon.dragMode = DragMode::PendingOutsideSelection;
            g_Daemon.dragStartPt = pt;
            bool ctrlHeldOnDown = ((GetKeyState(VK_CONTROL) & 0x8000) != 0) ||
                                  ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) ||
                                  ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0) ||
                                  ((GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0);
            bool shiftHeldOnDown = ((GetKeyState(VK_SHIFT) & 0x8000) != 0) ||
                                   ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) ||
                                   ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0) ||
                                   ((GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0);
            bool altHeldOnDown = PepperSnapDaemon::IsAltModifierHeld();
            g_Daemon.altHeldOnSelectionStart = altHeldOnDown;
            if (!g_Daemon.hasSelection && shiftHeldOnDown && !ctrlHeldOnDown && !altHeldOnDown) {
                g_Daemon.pendingShiftColorPick = true;
                g_Daemon.pendingCtrlWindowSelect = false;
            } else if (!g_Daemon.hasSelection && ctrlHeldOnDown && !altHeldOnDown) {
                g_Daemon.pendingShiftColorPick = false;
                g_Daemon.UpdateCtrlWindowHover();
                g_Daemon.pendingCtrlWindowSelect = g_Daemon.hasCtrlHoverWindow;
                g_Daemon.pendingCtrlWindowRect = g_Daemon.ctrlHoverWindowRect;
            } else {
                g_Daemon.pendingShiftColorPick = false;
                g_Daemon.pendingCtrlWindowSelect = false;
            }
            SetCapture(hWnd);
            g_Daemon.UpdateOverlayCursor(mx, my);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        case WM_MOUSEMOVE: {
            bool isFreehandDrawing = (g_Daemon.dragMode == DragMode::DrawingAnnotation &&
                                     (g_Daemon.draftAnn.tool == OverlayTool::Pen || g_Daemon.draftAnn.tool == OverlayTool::Highlighter));
            if (!isFreehandDrawing) {
                // Coalesce queued WM_MOUSEMOVE events for non-freehand drags so high-polling-rate mice never lag
                MSG nextMove;
                while (PeekMessageW(&nextMove, hWnd, WM_MOUSEMOVE, WM_MOUSEMOVE, PM_REMOVE)) {
                    lParam = nextMove.lParam;
                    wParam = nextMove.wParam;
                }
            }
            int mx = GET_X_LPARAM(lParam);
            int my = GET_Y_LPARAM(lParam);
            POINT pt{ mx, my };
            g_Daemon.mousePt = pt;

            if (g_Daemon.pressedBtnId != -1) {
                int newHover = -1;
                for (const auto& b : g_Daemon.dockButtons) {
                    if (b.id == g_Daemon.pressedBtnId && PtInRect(&b.rect, pt)) {
                        newHover = b.id;
                        break;
                    }
                }
                if (newHover != g_Daemon.hoveredBtnId) {
                    g_Daemon.hoveredBtnId = newHover;
                    g_Daemon.UpdateOverlayCursor(mx, my);
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                return 0;
            }

            if (g_Daemon.isDraggingTextBoxSel && g_Daemon.isEditingText) {
                g_Daemon.textCaretPos = g_Daemon.HitTestTextBoxIndex(mx, my);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (g_Daemon.isDraggingStrokeText) {
                g_Daemon.strokeCaretPos = PepperSnapDaemon::HitTestMonoIndex(
                    mx, g_Daemon.customStrokeRect, g_Daemon.editingStrokeText, L" px"
                );
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (g_Daemon.isDraggingSizeText) {
                g_Daemon.sizeCaretPos = PepperSnapDaemon::HitTestMonoIndex(
                    mx, g_Daemon.dimPillRect, g_Daemon.editingSizeText, L" px"
                );
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (g_Daemon.isDraggingRatioText) {
                g_Daemon.ratioCaretPos = PepperSnapDaemon::HitTestMonoIndex(
                    mx, g_Daemon.ratioPillRect, g_Daemon.editingRatioText, L""
                );
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }

            if (g_Daemon.dragMode == DragMode::None) {
                if (!g_Daemon.hasSelection) {
                    g_Daemon.UpdateCtrlWindowHover();
                } else {
                    g_Daemon.hasCtrlHoverWindow = false;
                }
                bool overFrameStripUI = false;
                bool frameStripHoverChanged = false;
                if (g_Daemon.HasOverlayMultiFrames()) {
                    g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                    bool inToggleSq = (PtInRect(&g_Daemon.overlayFrameStripToggleBtnRect, pt) != FALSE);
                    bool newPlayHov   = (inToggleSq && PtInRect(&g_Daemon.overlayFrameStripPlayBtnRect, pt) != FALSE);
                    bool newToggleHov = (inToggleSq && !newPlayHov);
                    bool newResizeHov = (!g_Daemon.overlayFrameStripCollapsed && !inToggleSq && PtInRect(&g_Daemon.overlayFrameStripResizeRect, pt) != FALSE);
                    bool newScrollHov = (!g_Daemon.overlayFrameStripCollapsed && PtInRect(&g_Daemon.overlayFrameStripScrollTrackRect, pt) != FALSE);
                    bool newLeftHov   = (!g_Daemon.overlayFrameStripCollapsed && PtInRect(&g_Daemon.overlayFrameStripScrollLeftRect, pt) != FALSE);
                    bool newRightHov  = (!g_Daemon.overlayFrameStripCollapsed && PtInRect(&g_Daemon.overlayFrameStripScrollRightRect, pt) != FALSE);
                    int newCardHov = -1;
                    if (!g_Daemon.overlayFrameStripCollapsed && !newResizeHov && !newScrollHov && !newLeftHov && !newRightHov &&
                        PtInRect(&g_Daemon.overlayFrameStripPanelRect, pt)) {
                        for (size_t i = 0; i < g_Daemon.overlayFrameCardRects.size(); ++i) {
                            if (PtInRect(&g_Daemon.overlayFrameCardRects[i], pt)) {
                                newCardHov = (int)i;
                                break;
                            }
                        }
                    }
                    overFrameStripUI = inToggleSq || newResizeHov ||
                                       (!g_Daemon.overlayFrameStripCollapsed && PtInRect(&g_Daemon.overlayFrameStripPanelRect, pt));
                    if (newToggleHov != g_Daemon.overlayToggleBtnHovered ||
                        newPlayHov != g_Daemon.overlayPlayBtnHovered ||
                        newResizeHov != g_Daemon.overlayTopResizeHovered ||
                        newScrollHov != g_Daemon.overlayScrollbarHovered ||
                        newLeftHov != g_Daemon.overlayScrollLeftBtnHovered ||
                        newRightHov != g_Daemon.overlayScrollRightBtnHovered ||
                        newCardHov != g_Daemon.overlayHoveredFrameIdx) {
                        frameStripHoverChanged = true;
                        g_Daemon.overlayToggleBtnHovered = newToggleHov;
                        g_Daemon.overlayPlayBtnHovered = newPlayHov;
                        g_Daemon.overlayTopResizeHovered = newResizeHov;
                        g_Daemon.overlayScrollbarHovered = newScrollHov;
                        g_Daemon.overlayScrollLeftBtnHovered = newLeftHov;
                        g_Daemon.overlayScrollRightBtnHovered = newRightHov;
                        g_Daemon.overlayHoveredFrameIdx = newCardHov;
                    }
                }
                int newHover = -1;
                if (!overFrameStripUI) {
                    for (const auto& b : g_Daemon.dockButtons) {
                        if (PtInRect(&b.rect, pt)) {
                            newHover = b.id;
                            break;
                        }
                    }
                }
                bool pillHover = !overFrameStripUI && g_Daemon.hasSelection &&
                                 (PtInRect(&g_Daemon.dimPillRect, pt) != FALSE || PtInRect(&g_Daemon.ratioPillRect, pt) != FALSE);
                bool ratioResetHover = !overFrameStripUI && g_Daemon.hasSelection && g_Daemon.hasCustomRatio &&
                                       (PtInRect(&g_Daemon.ratioResetBtnRect, pt) != FALSE);
                bool strokeHover = !overFrameStripUI && g_Daemon.hasSelection && ( PtInRect(&g_Daemon.customStrokeRect, pt) != FALSE );
                bool hoverChanged = frameStripHoverChanged ||
                                    (newHover != g_Daemon.hoveredBtnId) ||
                                    (pillHover != g_Daemon.lastPillHover) ||
                                    (ratioResetHover != g_Daemon.lastRatioResetHover) ||
                                    (strokeHover != g_Daemon.lastStrokeHover);
                g_Daemon.hoveredBtnId = newHover;
                g_Daemon.lastPillHover = pillHover;
                g_Daemon.lastRatioResetHover = ratioResetHover;
                g_Daemon.lastStrokeHover = strokeHover;
                if (g_Daemon.IsHoveringSelectedCustomArea(pt)) {
                    bool movedHoverMouse = (pt.x != g_Daemon.lastCustomAreaHoverMouse.x || pt.y != g_Daemon.lastCustomAreaHoverMouse.y);
                    g_Daemon.lastCustomAreaHoverMouse = pt;
                    if (!g_Daemon.showingCustomAreaHoverTip && movedHoverMouse) {
                        SetTimer(hWnd, 4, 500, nullptr);
                    }
                } else {
                    KillTimer(hWnd, 4);
                    g_Daemon.lastCustomAreaHoverMouse = { -10000, -10000 };
                    if (g_Daemon.showingCustomAreaHoverTip) {
                        g_Daemon.showingCustomAreaHoverTip = false;
                        hoverChanged = true;
                    }
                }
                g_Daemon.UpdateOverlayCursor(mx, my);
                if (!g_Daemon.hasSelection || hoverChanged) {
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                return 0;
            }

            KillTimer(hWnd, 4);
            g_Daemon.showingCustomAreaHoverTip = false;
            int clampedMx = std::max(0, std::min(g_Daemon.vScreenW, mx));
            int clampedMy = std::max(0, std::min(g_Daemon.vScreenH, my));
            int dx = mx - g_Daemon.dragStartPt.x;
            int dy = my - g_Daemon.dragStartPt.y;
            g_Daemon.isRightMouseHeld = ((wParam & MK_RBUTTON) != 0);
            int ratioW = 0, ratioH = 0;
            bool isTempRatioOverride = false;
            bool hasRatioConstrain = !g_Daemon.isDraggingSingleCorner &&
                                     g_Daemon.GetActiveSelectionAspectRatio(wParam, ratioW, ratioH, &isTempRatioOverride);
            if (g_Daemon.dragMode == DragMode::CreatingSelection ||
                (g_Daemon.dragMode >= DragMode::ResizeTL && g_Daemon.dragMode <= DragMode::ResizeL)) {
                g_Daemon.activeSelectionRatioW = (hasRatioConstrain && isTempRatioOverride) ? ratioW : 0;
                g_Daemon.activeSelectionRatioH = (hasRatioConstrain && isTempRatioOverride) ? ratioH : 0;
            }

            auto applyRatioCorner = [&](int anchorX, int anchorY) {
                int sdx = clampedMx - anchorX;
                int sdy = clampedMy - anchorY;
                int w = 0, h = 0;
                if ((long long)std::abs(sdx) * ratioH >= (long long)std::abs(sdy) * ratioW) {
                    w = std::abs(sdx);
                    h = (int)std::round((double)w * (double)ratioH / (double)ratioW);
                } else {
                    h = std::abs(sdy);
                    w = (int)std::round((double)h * (double)ratioW / (double)ratioH);
                }
                int maxW = (sdx >= 0) ? (g_Daemon.vScreenW - anchorX) : anchorX;
                int maxH = (sdy >= 0) ? (g_Daemon.vScreenH - anchorY) : anchorY;
                if (w > maxW) {
                    w = maxW;
                    h = (int)std::round((double)w * (double)ratioH / (double)ratioW);
                }
                if (h > maxH) {
                    h = maxH;
                    w = (int)std::round((double)h * (double)ratioW / (double)ratioH);
                }
                w = std::min(w, maxW);
                h = std::min(h, maxH);
                int tx = anchorX + (sdx >= 0 ? w : -w);
                int ty = anchorY + (sdy >= 0 ? h : -h);
                g_Daemon.selRect = { std::min(anchorX, tx), std::min(anchorY, ty), std::max(anchorX, tx), std::max(anchorY, ty) };
                g_Daemon.selectionTargetPt = { tx, ty };
            };

            auto applyRatioEdgeY = [&](int anchorX, int anchorY) {
                int sdy = clampedMy - anchorY;
                int h = std::abs(sdy);
                int w = (int)std::round((double)h * (double)ratioW / (double)ratioH);
                int maxW = g_Daemon.vScreenW - anchorX;
                int maxH = (sdy >= 0) ? (g_Daemon.vScreenH - anchorY) : anchorY;
                if (h > maxH) {
                    h = maxH;
                    w = (int)std::round((double)h * (double)ratioW / (double)ratioH);
                }
                if (w > maxW) {
                    w = maxW;
                    h = (int)std::round((double)w * (double)ratioH / (double)ratioW);
                }
                w = std::min(w, maxW);
                h = std::min(h, maxH);
                int ty = anchorY + (sdy >= 0 ? h : -h);
                int tx = anchorX + w;
                g_Daemon.selRect = { anchorX, std::min(anchorY, ty), tx, std::max(anchorY, ty) };
                g_Daemon.selectionTargetPt = { tx, ty };
            };

            auto applyRatioEdgeX = [&](int anchorX, int anchorY) {
                int sdx = clampedMx - anchorX;
                int w = std::abs(sdx);
                int h = (int)std::round((double)w * (double)ratioH / (double)ratioW);
                int maxW = (sdx >= 0) ? (g_Daemon.vScreenW - anchorX) : anchorX;
                int maxH = g_Daemon.vScreenH - anchorY;
                if (w > maxW) {
                    w = maxW;
                    h = (int)std::round((double)w * (double)ratioH / (double)ratioW);
                }
                if (h > maxH) {
                    h = maxH;
                    w = (int)std::round((double)h * (double)ratioW / (double)ratioH);
                }
                w = std::min(w, maxW);
                h = std::min(h, maxH);
                int tx = anchorX + (sdx >= 0 ? w : -w);
                int ty = anchorY + h;
                g_Daemon.selRect = { std::min(anchorX, tx), anchorY, std::max(anchorX, tx), ty };
                g_Daemon.selectionTargetPt = { tx, ty };
            };

            switch (g_Daemon.dragMode) {
                case DragMode::ResizingFrameStrip: {
                    int maxStripH = std::max(160, (int)(g_Daemon.vScreenH * 0.48));
                    int newH = g_Daemon.dragFrameStripOrigH + (g_Daemon.dragFrameStripStartY - my);
                    g_Daemon.overlayFrameStripHeight = std::max(92, std::min(maxStripH, newH));
                    g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                    break;
                }
                case DragMode::ScrollingFrameStrip: {
                    int trackW = std::max(1, (int)(g_Daemon.overlayFrameStripScrollTrackRect.right - g_Daemon.overlayFrameStripScrollTrackRect.left));
                    int thumbW = std::max(24, (int)(g_Daemon.overlayFrameStripScrollThumbRect.right - g_Daemon.overlayFrameStripScrollThumbRect.left));
                    int usableTrack = std::max(1, trackW - thumbW);
                    float deltaScroll = ((float)(mx - g_Daemon.dragFrameScrollStartX) / (float)usableTrack) * g_Daemon.overlayFrameMaxScrollX;
                    g_Daemon.overlayFrameScrollX = std::max(0.0f, std::min(g_Daemon.overlayFrameMaxScrollX, g_Daemon.dragFrameScrollOrigX + deltaScroll));
                    g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                    break;
                }
                case DragMode::DraggingHUD: {
                    int totalW = std::max(100, (int)(g_Daemon.hudBoundsRect.right - g_Daemon.hudBoundsRect.left));
                    int totalH = std::max(40, (int)(g_Daemon.hudBoundsRect.bottom - g_Daemon.hudBoundsRect.top));
                    int newRightX = g_Daemon.dragHudOrigPos.x + (mx - g_Daemon.dragHudStartMouse.x);
                    int newTopY   = g_Daemon.dragHudOrigPos.y + (my - g_Daemon.dragHudStartMouse.y);
                    newRightX = std::max(totalW + 6, std::min(g_Daemon.vScreenW - 6, newRightX));
                    newTopY   = std::max(6, std::min(g_Daemon.vScreenH - totalH - 6, newTopY));
                    g_Daemon.customHudPos = { newRightX, newTopY };
                    g_Daemon.hasCustomHudPos = true;
                    break;
                }
                case DragMode::PendingOutsideSelection: {
                    bool altHeldMove = PepperSnapDaemon::IsAltModifierHeld();
                    if (!hasRatioConstrain) {
                        bool ctrlHeldMove = ((GetKeyState(VK_CONTROL) & 0x8000) != 0) ||
                                            ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) ||
                                            ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0) ||
                                            ((GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0);
                        if (!g_Daemon.hasSelection && g_Daemon.pendingShiftColorPick && !ctrlHeldMove && !altHeldMove) {
                            InvalidateRect(hWnd, nullptr, FALSE);
                            return 0;
                        }
                        if (!g_Daemon.hasSelection && ctrlHeldMove && !altHeldMove) {
                            if (g_Daemon.UpdateCtrlWindowHover()) {
                                g_Daemon.pendingCtrlWindowSelect = g_Daemon.hasCtrlHoverWindow;
                                g_Daemon.pendingCtrlWindowRect = g_Daemon.ctrlHoverWindowRect;
                                g_Daemon.UpdateOverlayCursor(mx, my);
                                InvalidateRect(hWnd, nullptr, FALSE);
                            }
                            return 0;
                        }
                    }
                    if (std::abs(dx) >= 5 || std::abs(dy) >= 5) {
                        g_Daemon.pendingShiftColorPick = false;
                        g_Daemon.pendingCtrlWindowSelect = false;
                        if (g_Daemon.autoHideFrameList && g_Daemon.HasOverlayMultiFrames() && !g_Daemon.overlayFrameStripCollapsed) {
                            g_Daemon.overlayFrameStripCollapsed = true;
                            g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                        }
                        g_Daemon.hasSelection = true;
                        g_Daemon.hasEllipseSelection = g_Daemon.altHeldOnSelectionStart || altHeldMove;
                        g_Daemon.hasCustomHudPos = false;
                        g_Daemon.activeTool = OverlayTool::SelectMove;
                        g_Daemon.annotations.clear();
                        g_Daemon.undoStack.clear();
                        g_Daemon.redoStack.clear();
                        g_Daemon.nextStepNum = 1;
                        g_Daemon.ClearAnnotationSelection();
                        g_Daemon.dragMode = DragMode::CreatingSelection;
                        int startX = std::max(0, std::min(g_Daemon.vScreenW, (int)g_Daemon.dragStartPt.x));
                        int startY = std::max(0, std::min(g_Daemon.vScreenH, (int)g_Daemon.dragStartPt.y));
                        g_Daemon.dragOrigRect = { startX, startY, startX, startY };
                        hasRatioConstrain = !g_Daemon.isDraggingSingleCorner &&
                                            g_Daemon.GetActiveSelectionAspectRatio(wParam, ratioW, ratioH, &isTempRatioOverride);
                        g_Daemon.activeSelectionRatioW = (hasRatioConstrain && isTempRatioOverride) ? ratioW : 0;
                        g_Daemon.activeSelectionRatioH = (hasRatioConstrain && isTempRatioOverride) ? ratioH : 0;
                        if (hasRatioConstrain) {
                            applyRatioCorner(startX, startY);
                        } else {
                            g_Daemon.selRect = { startX, startY, clampedMx, clampedMy };
                            g_Daemon.selectionTargetPt = { clampedMx, clampedMy };
                        }
                        g_Daemon.SyncQuadFromSelRect();
                    } else {
                        return 0;
                    }
                    break;
                }
                case DragMode::CreatingSelection:
                    g_Daemon.hasCustomHudPos = false;
                    g_Daemon.hasEllipseSelection = g_Daemon.altHeldOnSelectionStart || PepperSnapDaemon::IsAltModifierHeld();
                    if (hasRatioConstrain) {
                        int startX = std::max(0, std::min(g_Daemon.vScreenW, (int)g_Daemon.dragStartPt.x));
                        int startY = std::max(0, std::min(g_Daemon.vScreenH, (int)g_Daemon.dragStartPt.y));
                        applyRatioCorner(startX, startY);
                    } else {
                        int startX = std::max(0, std::min(g_Daemon.vScreenW, (int)g_Daemon.dragStartPt.x));
                        int startY = std::max(0, std::min(g_Daemon.vScreenH, (int)g_Daemon.dragStartPt.y));
                        g_Daemon.selRect = { startX, startY, clampedMx, clampedMy };
                        g_Daemon.selectionTargetPt = { clampedMx, clampedMy };
                    }
                    g_Daemon.SyncQuadFromSelRect();
                    break;
                case DragMode::MovingSelection: {
                    g_Daemon.hasCustomHudPos = false;
                    int cDx = std::max((int)(-g_Daemon.dragOrigRect.left), std::min((int)(g_Daemon.vScreenW - g_Daemon.dragOrigRect.right), dx));
                    int cDy = std::max((int)(-g_Daemon.dragOrigRect.top),  std::min((int)(g_Daemon.vScreenH - g_Daemon.dragOrigRect.bottom), dy));
                    g_Daemon.selRect.left   = g_Daemon.dragOrigRect.left + cDx;
                    g_Daemon.selRect.top    = g_Daemon.dragOrigRect.top + cDy;
                    g_Daemon.selRect.right  = g_Daemon.dragOrigRect.right + cDx;
                    g_Daemon.selRect.bottom = g_Daemon.dragOrigRect.bottom + cDy;
                    for (int i = 0; i < 4; ++i) {
                        g_Daemon.selQuad[i].x = g_Daemon.dragOrigQuad[i].x + cDx;
                        g_Daemon.selQuad[i].y = g_Daemon.dragOrigQuad[i].y + cDy;
                    }
                    break;
                }
                case DragMode::ResizeTL:
                case DragMode::ResizeT:
                case DragMode::ResizeTR:
                case DragMode::ResizeR:
                case DragMode::ResizeBR:
                case DragMode::ResizeB:
                case DragMode::ResizeBL:
                case DragMode::ResizeL: {
                    g_Daemon.hasCustomHudPos = false;
                    if (g_Daemon.isDraggingSingleCorner) {
                        for (int i = 0; i < 4; ++i) g_Daemon.selQuad[i] = g_Daemon.dragOrigQuad[i];
                        int cornerIdx = 0;
                        if (g_Daemon.dragMode == DragMode::ResizeTL) cornerIdx = 0;
                        else if (g_Daemon.dragMode == DragMode::ResizeTR) cornerIdx = 1;
                        else if (g_Daemon.dragMode == DragMode::ResizeBR) cornerIdx = 2;
                        else if (g_Daemon.dragMode == DragMode::ResizeBL) cornerIdx = 3;

                        int targetX = clampedMx;
                        int targetY = clampedMy;
                        const double kCornerSnapDist = 12.0;
                        double bestDist = kCornerSnapDist + 0.001;
                        int snappedOtherIdx = -1;
                        for (int i = 0; i < 4; ++i) {
                            if (i == cornerIdx) continue;
                            bool startedCoincident = (g_Daemon.dragOrigQuad[i].x == g_Daemon.dragOrigQuad[cornerIdx].x &&
                                                      g_Daemon.dragOrigQuad[i].y == g_Daemon.dragOrigQuad[cornerIdx].y);
                            double maxSnap = startedCoincident ? 2.5 : kCornerSnapDist;
                            double d = std::hypot((double)(clampedMx - g_Daemon.dragOrigQuad[i].x),
                                                  (double)(clampedMy - g_Daemon.dragOrigQuad[i].y));
                            if (d <= maxSnap && d < bestDist) {
                                POINT testQuad[4];
                                for (int k = 0; k < 4; ++k) testQuad[k] = g_Daemon.dragOrigQuad[k];
                                testQuad[cornerIdx] = g_Daemon.dragOrigQuad[i];
                                int uniqueCnt = 0;
                                POINT uniq[4];
                                for (int k = 0; k < 4; ++k) {
                                    bool dup = false;
                                    for (int u = 0; u < uniqueCnt; ++u) {
                                        if (testQuad[k].x == uniq[u].x && testQuad[k].y == uniq[u].y) {
                                            dup = true;
                                            break;
                                        }
                                    }
                                    if (!dup) uniq[uniqueCnt++] = testQuad[k];
                                }
                                if (uniqueCnt >= 3) {
                                    bestDist = d;
                                    snappedOtherIdx = i;
                                }
                            }
                        }
                        if (snappedOtherIdx >= 0) {
                            targetX = g_Daemon.dragOrigQuad[snappedOtherIdx].x;
                            targetY = g_Daemon.dragOrigQuad[snappedOtherIdx].y;
                        }

                        int moveIdx = cornerIdx;
                        int adjCoincidentIdx = -1;
                        int prevIdx = (cornerIdx + 3) & 3;
                        int nextIdx = (cornerIdx + 1) & 3;
                        if (g_Daemon.dragOrigQuad[prevIdx].x == g_Daemon.dragOrigQuad[cornerIdx].x &&
                            g_Daemon.dragOrigQuad[prevIdx].y == g_Daemon.dragOrigQuad[cornerIdx].y) {
                            adjCoincidentIdx = prevIdx;
                        } else if (g_Daemon.dragOrigQuad[nextIdx].x == g_Daemon.dragOrigQuad[cornerIdx].x &&
                                   g_Daemon.dragOrigQuad[nextIdx].y == g_Daemon.dragOrigQuad[cornerIdx].y) {
                            adjCoincidentIdx = nextIdx;
                        }
                        if (adjCoincidentIdx >= 0 &&
                            (targetX != g_Daemon.dragOrigQuad[cornerIdx].x || targetY != g_Daemon.dragOrigQuad[cornerIdx].y)) {
                            POINT candA[4], candB[4];
                            for (int i = 0; i < 4; ++i) {
                                candA[i] = g_Daemon.dragOrigQuad[i];
                                candB[i] = g_Daemon.dragOrigQuad[i];
                            }
                            candA[cornerIdx] = { targetX, targetY };
                            candB[adjCoincidentIdx] = { targetX, targetY };
                            bool interA = PepperSnapDaemon::IsQuadSelfIntersecting(candA);
                            bool interB = PepperSnapDaemon::IsQuadSelfIntersecting(candB);
                            if (interA && !interB) {
                                moveIdx = adjCoincidentIdx;
                            } else if (!interA && !interB) {
                                if (std::llabs(PepperSnapDaemon::QuadSignedArea2(candB)) >
                                    std::llabs(PepperSnapDaemon::QuadSignedArea2(candA))) {
                                    moveIdx = adjCoincidentIdx;
                                }
                            }
                        }

                        g_Daemon.selQuad[moveIdx] = { targetX, targetY };
                        g_Daemon.selectionTargetPt = { targetX, targetY };
                        g_Daemon.SyncSelRectFromQuad();
                    } else if (g_Daemon.hasCustomQuad && !hasRatioConstrain) {
                        for (int i = 0; i < 4; ++i) g_Daemon.selQuad[i] = g_Daemon.dragOrigQuad[i];
                        auto clampX = [&](int v) { return std::max(0, std::min(g_Daemon.vScreenW, v)); };
                        auto clampY = [&](int v) { return std::max(0, std::min(g_Daemon.vScreenH, v)); };
                        switch (g_Daemon.dragMode) {
                            case DragMode::ResizeTL: {
                                int cdx = clampedMx - g_Daemon.dragOrigQuad[0].x;
                                int cdy = clampedMy - g_Daemon.dragOrigQuad[0].y;
                                g_Daemon.selQuad[0] = { clampedMx, clampedMy };
                                g_Daemon.selQuad[1].y = clampY(g_Daemon.dragOrigQuad[1].y + cdy);
                                g_Daemon.selQuad[3].x = clampX(g_Daemon.dragOrigQuad[3].x + cdx);
                                break;
                            }
                            case DragMode::ResizeTR: {
                                int cdx = clampedMx - g_Daemon.dragOrigQuad[1].x;
                                int cdy = clampedMy - g_Daemon.dragOrigQuad[1].y;
                                g_Daemon.selQuad[1] = { clampedMx, clampedMy };
                                g_Daemon.selQuad[0].y = clampY(g_Daemon.dragOrigQuad[0].y + cdy);
                                g_Daemon.selQuad[2].x = clampX(g_Daemon.dragOrigQuad[2].x + cdx);
                                break;
                            }
                            case DragMode::ResizeBR: {
                                int cdx = clampedMx - g_Daemon.dragOrigQuad[2].x;
                                int cdy = clampedMy - g_Daemon.dragOrigQuad[2].y;
                                g_Daemon.selQuad[2] = { clampedMx, clampedMy };
                                g_Daemon.selQuad[1].x = clampX(g_Daemon.dragOrigQuad[1].x + cdx);
                                g_Daemon.selQuad[3].y = clampY(g_Daemon.dragOrigQuad[3].y + cdy);
                                break;
                            }
                            case DragMode::ResizeBL: {
                                int cdx = clampedMx - g_Daemon.dragOrigQuad[3].x;
                                int cdy = clampedMy - g_Daemon.dragOrigQuad[3].y;
                                g_Daemon.selQuad[3] = { clampedMx, clampedMy };
                                g_Daemon.selQuad[0].x = clampX(g_Daemon.dragOrigQuad[0].x + cdx);
                                g_Daemon.selQuad[2].y = clampY(g_Daemon.dragOrigQuad[2].y + cdy);
                                break;
                            }
                            case DragMode::ResizeT:
                                g_Daemon.selQuad[0].y = clampY(g_Daemon.dragOrigQuad[0].y + dy);
                                g_Daemon.selQuad[1].y = clampY(g_Daemon.dragOrigQuad[1].y + dy);
                                break;
                            case DragMode::ResizeR:
                                g_Daemon.selQuad[1].x = clampX(g_Daemon.dragOrigQuad[1].x + dx);
                                g_Daemon.selQuad[2].x = clampX(g_Daemon.dragOrigQuad[2].x + dx);
                                break;
                            case DragMode::ResizeB:
                                g_Daemon.selQuad[2].y = clampY(g_Daemon.dragOrigQuad[2].y + dy);
                                g_Daemon.selQuad[3].y = clampY(g_Daemon.dragOrigQuad[3].y + dy);
                                break;
                            case DragMode::ResizeL:
                                g_Daemon.selQuad[3].x = clampX(g_Daemon.dragOrigQuad[3].x + dx);
                                g_Daemon.selQuad[0].x = clampX(g_Daemon.dragOrigQuad[0].x + dx);
                                break;
                            default:
                                break;
                        }
                        // Preserve any snapped/coincident corner pairs during non-Ctrl resize
                        for (int i = 0; i < 4; ++i) {
                            for (int j = i + 1; j < 4; ++j) {
                                if (g_Daemon.dragOrigQuad[i].x == g_Daemon.dragOrigQuad[j].x &&
                                    g_Daemon.dragOrigQuad[i].y == g_Daemon.dragOrigQuad[j].y) {
                                    bool iMoved = (g_Daemon.selQuad[i].x != g_Daemon.dragOrigQuad[i].x ||
                                                   g_Daemon.selQuad[i].y != g_Daemon.dragOrigQuad[i].y);
                                    if (iMoved) {
                                        g_Daemon.selQuad[j] = g_Daemon.selQuad[i];
                                    } else {
                                        g_Daemon.selQuad[i] = g_Daemon.selQuad[j];
                                    }
                                }
                            }
                        }
                        g_Daemon.selectionTargetPt = { clampedMx, clampedMy };
                        g_Daemon.SyncSelRectFromQuad();
                    } else {
                        switch (g_Daemon.dragMode) {
                            case DragMode::ResizeTL:
                                if (hasRatioConstrain) applyRatioCorner(g_Daemon.dragOrigRect.right, g_Daemon.dragOrigRect.bottom);
                                else {
                                    g_Daemon.selRect = { clampedMx, clampedMy, g_Daemon.dragOrigRect.right, g_Daemon.dragOrigRect.bottom };
                                    g_Daemon.selectionTargetPt = { clampedMx, clampedMy };
                                }
                                break;
                            case DragMode::ResizeT:
                                if (hasRatioConstrain) applyRatioEdgeY(g_Daemon.dragOrigRect.left, g_Daemon.dragOrigRect.bottom);
                                else {
                                    g_Daemon.selRect = { g_Daemon.dragOrigRect.left, clampedMy, g_Daemon.dragOrigRect.right, g_Daemon.dragOrigRect.bottom };
                                    g_Daemon.selectionTargetPt = { clampedMx, clampedMy };
                                }
                                break;
                            case DragMode::ResizeTR:
                                if (hasRatioConstrain) applyRatioCorner(g_Daemon.dragOrigRect.left, g_Daemon.dragOrigRect.bottom);
                                else {
                                    g_Daemon.selRect = { g_Daemon.dragOrigRect.left, clampedMy, clampedMx, g_Daemon.dragOrigRect.bottom };
                                    g_Daemon.selectionTargetPt = { clampedMx, clampedMy };
                                }
                                break;
                            case DragMode::ResizeR:
                                if (hasRatioConstrain) applyRatioEdgeX(g_Daemon.dragOrigRect.left, g_Daemon.dragOrigRect.top);
                                else {
                                    g_Daemon.selRect = { g_Daemon.dragOrigRect.left, g_Daemon.dragOrigRect.top, clampedMx, g_Daemon.dragOrigRect.bottom };
                                    g_Daemon.selectionTargetPt = { clampedMx, clampedMy };
                                }
                                break;
                            case DragMode::ResizeBR:
                                if (hasRatioConstrain) applyRatioCorner(g_Daemon.dragOrigRect.left, g_Daemon.dragOrigRect.top);
                                else {
                                    g_Daemon.selRect = { g_Daemon.dragOrigRect.left, g_Daemon.dragOrigRect.top, clampedMx, clampedMy };
                                    g_Daemon.selectionTargetPt = { clampedMx, clampedMy };
                                }
                                break;
                            case DragMode::ResizeB:
                                if (hasRatioConstrain) applyRatioEdgeY(g_Daemon.dragOrigRect.left, g_Daemon.dragOrigRect.top);
                                else {
                                    g_Daemon.selRect = { g_Daemon.dragOrigRect.left, g_Daemon.dragOrigRect.top, g_Daemon.dragOrigRect.right, clampedMy };
                                    g_Daemon.selectionTargetPt = { clampedMx, clampedMy };
                                }
                                break;
                            case DragMode::ResizeBL:
                                if (hasRatioConstrain) applyRatioCorner(g_Daemon.dragOrigRect.right, g_Daemon.dragOrigRect.top);
                                else {
                                    g_Daemon.selRect = { clampedMx, g_Daemon.dragOrigRect.top, g_Daemon.dragOrigRect.right, clampedMy };
                                    g_Daemon.selectionTargetPt = { clampedMx, clampedMy };
                                }
                                break;
                            case DragMode::ResizeL:
                                if (hasRatioConstrain) applyRatioEdgeX(g_Daemon.dragOrigRect.right, g_Daemon.dragOrigRect.top);
                                else {
                                    g_Daemon.selRect = { clampedMx, g_Daemon.dragOrigRect.top, g_Daemon.dragOrigRect.right, g_Daemon.dragOrigRect.bottom };
                                    g_Daemon.selectionTargetPt = { clampedMx, clampedMy };
                                }
                                break;
                            default:
                                break;
                        }
                        if (g_Daemon.hasCustomQuad) {
                            g_Daemon.ScaleQuadFromOrigRect(g_Daemon.dragOrigQuad, g_Daemon.dragOrigRect, g_Daemon.selRect);
                        } else {
                            g_Daemon.SyncQuadFromSelRect();
                        }
                    }
                    break;
                }
                case DragMode::DrawingAnnotation: {
                    bool shiftHeld = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                    float targetX = (float)mx;
                    float targetY = (float)my;
                    OverlayTool t = g_Daemon.draftAnn.tool;

                    if (t == OverlayTool::Pen || t == OverlayTool::Highlighter) {
                        // Snapping in Pen & Highlighter uses the LAST point (not the first point) as anchor!
                        if (shiftHeld) {
                            if (!g_Daemon.isPenShiftSnapping) {
                                g_Daemon.penSnapAnchor = g_Daemon.draftAnn.points.empty()
                                    ? g_Daemon.draftAnn.startPt
                                    : g_Daemon.draftAnn.points.back();
                                g_Daemon.isPenShiftSnapping = true;
                                g_Daemon.draftAnn.points.push_back(g_Daemon.penSnapAnchor);
                            }
                            float sdx = targetX - g_Daemon.penSnapAnchor.x;
                            float sdy = targetY - g_Daemon.penSnapAnchor.y;
                            float dist = std::hypot(sdx, sdy);
                            float angle = std::atan2(sdy, sdx);
                            const float step15 = 3.1415926535f / 12.0f; // 15 degrees in radians
                            float snappedAngle = std::round(angle / step15) * step15;
                            targetX = g_Daemon.penSnapAnchor.x + dist * std::cos(snappedAngle);
                            targetY = g_Daemon.penSnapAnchor.y + dist * std::sin(snappedAngle);
                            g_Daemon.draftAnn.points.back() = { targetX, targetY };
                        } else {
                            g_Daemon.isPenShiftSnapping = false;
                            auto recordFreehandPt = [&](float rx, float ry) {
                                if (g_Daemon.draftAnn.points.empty()) {
                                    g_Daemon.draftAnn.points.push_back({ rx, ry });
                                    g_Daemon.penSnapAnchor = { rx, ry };
                                } else {
                                    const auto& prev = g_Daemon.draftAnn.points.back();
                                    float pdx = rx - prev.x, pdy = ry - prev.y;
                                    if (!g_Daemon.penSmoothingEnabled || g_Daemon.penSmoothingStrength <= 0) {
                                        if (pdx * pdx + pdy * pdy >= 2.25f) {
                                            g_Daemon.draftAnn.points.push_back({ rx, ry });
                                            g_Daemon.penSnapAnchor = { rx, ry };
                                        }
                                    } else {
                                        float sNorm = std::max(0.05f, std::min(1.0f, g_Daemon.penSmoothingStrength / 100.0f));
                                        float minDist = 1.6f + 2.8f * sNorm;
                                        if (pdx * pdx + pdy * pdy >= minDist * minDist) {
                                            float prevW = 0.08f + 0.44f * sNorm;
                                            float curW  = 1.0f - prevW;
                                            PointF2D smoothed = {
                                                prev.x * prevW + rx * curW,
                                                prev.y * prevW + ry * curW
                                            };
                                            g_Daemon.draftAnn.points.push_back(smoothed);
                                            g_Daemon.penSnapAnchor = smoothed;
                                        }
                                    }
                                }
                            };
                            recordFreehandPt(targetX, targetY);
                            // Also drain any queued WM_MOUSEMOVE points into the freehand curve before repainting once
                            MSG nextMove;
                            while (PeekMessageW(&nextMove, hWnd, WM_MOUSEMOVE, WM_MOUSEMOVE, PM_REMOVE)) {
                                int nmx = GET_X_LPARAM(nextMove.lParam);
                                int nmy = GET_Y_LPARAM(nextMove.lParam);
                                g_Daemon.mousePt = { nmx, nmy };
                                float ntx = (float)nmx;
                                float nty = (float)nmy;
                                recordFreehandPt(ntx, nty);
                                targetX = ntx;
                                targetY = nty;
                            }
                        }
                        g_Daemon.draftAnn.endPt = { targetX, targetY };
                        break;
                    }

                    if (shiftHeld) {
                        float sdx = targetX - g_Daemon.draftAnn.startPt.x;
                        float sdy = targetY - g_Daemon.draftAnn.startPt.y;
                        if (t == OverlayTool::Line || t == OverlayTool::Arrow || t == OverlayTool::NumberArrow) {
                            // 15-degree angle snapping for Line, Arrow, and Numbering Arrow on Shift-drag
                            float dist = std::hypot(sdx, sdy);
                            float angle = std::atan2(sdy, sdx);
                            const float step15 = 3.1415926535f / 12.0f; // 15 degrees in radians
                            float snappedAngle = std::round(angle / step15) * step15;
                            targetX = g_Daemon.draftAnn.startPt.x + dist * std::cos(snappedAngle);
                            targetY = g_Daemon.draftAnn.startPt.y + dist * std::sin(snappedAngle);
                        } else if (t == OverlayTool::Rectangle || t == OverlayTool::Ellipse ||
                                   t == OverlayTool::MosaicSquare || t == OverlayTool::MosaicCircle) {
                            // 1:1 aspect ratio snapping for Rectangle, Circle, Mosaic Rectangle, and Mosaic Circle on Shift-drag
                            float side = std::max(std::abs(sdx), std::abs(sdy));
                            targetX = g_Daemon.draftAnn.startPt.x + (sdx >= 0.0f ? side : -side);
                            targetY = g_Daemon.draftAnn.startPt.y + (sdy >= 0.0f ? side : -side);
                        }
                    }

                    g_Daemon.draftAnn.endPt = { targetX, targetY };
                    break;
                }
                case DragMode::MarqueeSelectingAnnotations: {
                    g_Daemon.marqueeRect = {
                        std::min((LONG)g_Daemon.marqueeStartPt.x, (LONG)clampedMx),
                        std::min((LONG)g_Daemon.marqueeStartPt.y, (LONG)clampedMy),
                        std::max((LONG)g_Daemon.marqueeStartPt.x, (LONG)clampedMx),
                        std::max((LONG)g_Daemon.marqueeStartPt.y, (LONG)clampedMy)
                    };
                    g_Daemon.hasMarqueeBox = true;
                    g_Daemon.selectedAnnotationIds.clear();
                    g_Daemon.selectedAnnotationId = -1;
                    for (const auto& a : g_Daemon.annotations) {
                        if (PepperSnapDaemon::AnnotationIntersectsRect(a, g_Daemon.marqueeRect)) {
                            g_Daemon.selectedAnnotationIds.push_back(a.id);
                            g_Daemon.selectedAnnotationId = a.id;
                        }
                    }
                    break;
                }
                case DragMode::MovingAnnotation:
                case DragMode::DraggingAnnotationStart:
                case DragMode::DraggingAnnotationEnd: {
                    bool shiftHeld = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                    if (!g_Daemon.moveAnnUndoPushed) {
                        if (std::abs(mx - g_Daemon.dragStartPt.x) >= 1 || std::abs(my - g_Daemon.dragStartPt.y) >= 1) {
                            g_Daemon.PushUndo();
                            g_Daemon.moveAnnUndoPushed = true;
                            g_Daemon.clickedAlreadySelectedAnn = false;
                        } else {
                            break;
                        }
                    }
                    if (g_Daemon.dragMode == DragMode::DraggingAnnotationStart ||
                        g_Daemon.dragMode == DragMode::DraggingAnnotationEnd) {
                        for (auto& a : g_Daemon.annotations) {
                            if (a.id == g_Daemon.selectedAnnotationId) {
                                PointF2D anchor = (g_Daemon.dragMode == DragMode::DraggingAnnotationStart) ? a.endPt : a.startPt;
                                float tx = (float)mx, ty = (float)my;
                                if (shiftHeld) {
                                    float sdx = tx - anchor.x, sdy = ty - anchor.y;
                                    if (a.tool == OverlayTool::Line || a.tool == OverlayTool::Arrow || a.tool == OverlayTool::NumberArrow) {
                                        float dist = std::hypot(sdx, sdy);
                                        const float step15 = 3.1415926535f / 12.0f;
                                        float snappedAngle = std::round(std::atan2(sdy, sdx) / step15) * step15;
                                        tx = anchor.x + dist * std::cos(snappedAngle);
                                        ty = anchor.y + dist * std::sin(snappedAngle);
                                    } else if (a.tool == OverlayTool::Rectangle || a.tool == OverlayTool::Ellipse ||
                                               a.tool == OverlayTool::MosaicSquare || a.tool == OverlayTool::MosaicCircle) {
                                        float side = std::max(std::abs(sdx), std::abs(sdy));
                                        tx = anchor.x + (sdx >= 0.0f ? side : -side);
                                        ty = anchor.y + (sdy >= 0.0f ? side : -side);
                                    }
                                }
                                if (g_Daemon.dragMode == DragMode::DraggingAnnotationStart) a.startPt = { tx, ty };
                                else a.endPt = { tx, ty };
                                break;
                            }
                        }
                    } else {
                        float adx = 0.0f, ady = 0.0f;
                        bool foundPrimary = false;
                        for (const auto& a : g_Daemon.annotations) {
                            if (a.id == g_Daemon.selectedAnnotationId) {
                                adx = ((float)mx - g_Daemon.moveAnnOffset.x) - a.startPt.x;
                                ady = ((float)my - g_Daemon.moveAnnOffset.y) - a.startPt.y;
                                foundPrimary = true;
                                break;
                            }
                        }
                        if (foundPrimary) {
                            for (auto& a : g_Daemon.annotations) {
                                if (g_Daemon.IsAnnotationSelected(a.id)) {
                                    a.startPt.x += adx;
                                    a.startPt.y += ady;
                                    a.endPt.x   += adx;
                                    a.endPt.y   += ady;
                                    for (auto& p : a.points) { p.x += adx; p.y += ady; }
                                }
                            }
                        }
                    }
                    break;
                }
                default:
                    break;
            }
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        case WM_LBUTTONUP: {
            if (g_Daemon.overlayFramePressedItem != PepperSnapDaemon::FrameStripPressedItem::None) {
                auto pressedItem = g_Daemon.overlayFramePressedItem;
                int pressedCard = g_Daemon.overlayFramePressedCardIdx;
                g_Daemon.overlayFramePressedItem = PepperSnapDaemon::FrameStripPressedItem::None;
                g_Daemon.overlayFramePressedCardIdx = -1;
                ReleaseCapture();
                int mx = GET_X_LPARAM(lParam);
                int my = GET_Y_LPARAM(lParam);
                POINT pt{ mx, my };
                g_Daemon.mousePt = pt;
                if (g_Daemon.HasOverlayMultiFrames()) {
                    g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                    switch (pressedItem) {
                        case PepperSnapDaemon::FrameStripPressedItem::PlayPause:
                            if (PtInRect(&g_Daemon.overlayFrameStripToggleBtnRect, pt) &&
                                PtInRect(&g_Daemon.overlayFrameStripPlayBtnRect, pt)) {
                                g_Daemon.ToggleOverlayFramePlayback();
                            }
                            break;
                        case PepperSnapDaemon::FrameStripPressedItem::ToggleCollapse:
                            if (PtInRect(&g_Daemon.overlayFrameStripToggleBtnRect, pt) &&
                                !PtInRect(&g_Daemon.overlayFrameStripPlayBtnRect, pt)) {
                                g_Daemon.overlayFrameStripCollapsed = !g_Daemon.overlayFrameStripCollapsed;
                                if (!g_Daemon.overlayFrameStripCollapsed) {
                                    g_Daemon.EnsureActiveFrameVisibleInStrip();
                                }
                                g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                            }
                            break;
                        case PepperSnapDaemon::FrameStripPressedItem::ScrollLeft:
                            if (!g_Daemon.overlayFrameStripCollapsed &&
                                PtInRect(&g_Daemon.overlayFrameStripScrollLeftRect, pt)) {
                                g_Daemon.overlayFrameScrollX = std::max(0.0f, g_Daemon.overlayFrameScrollX - 160.0f);
                                g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                            }
                            break;
                        case PepperSnapDaemon::FrameStripPressedItem::ScrollRight:
                            if (!g_Daemon.overlayFrameStripCollapsed &&
                                PtInRect(&g_Daemon.overlayFrameStripScrollRightRect, pt)) {
                                g_Daemon.overlayFrameScrollX = std::min(g_Daemon.overlayFrameMaxScrollX, g_Daemon.overlayFrameScrollX + 160.0f);
                                g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                            }
                            break;
                        case PepperSnapDaemon::FrameStripPressedItem::FrameCard:
                            if (!g_Daemon.overlayFrameStripCollapsed &&
                                pressedCard >= 0 && pressedCard < (int)g_Daemon.overlayFrameCardRects.size() &&
                                PtInRect(&g_Daemon.overlayFrameCardRects[pressedCard], pt)) {
                                g_Daemon.StopOverlayFramePlayback();
                                g_Daemon.SelectOverlayFrame((size_t)pressedCard);
                            }
                            break;
                        default:
                            break;
                    }
                }
                g_Daemon.UpdateOverlayCursor(mx, my);
                if (IsWindow(hWnd)) {
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                return 0;
            }
            if (g_Daemon.pressedBtnId != -1) {
                int releasedBtnId = g_Daemon.pressedBtnId;
                g_Daemon.pressedBtnId = -1;
                ReleaseCapture();
                int mx = GET_X_LPARAM(lParam);
                int my = GET_Y_LPARAM(lParam);
                POINT pt{ mx, my };
                g_Daemon.mousePt = pt;
                const DockButton* hitBtn = nullptr;
                int newHover = -1;
                for (const auto& b : g_Daemon.dockButtons) {
                    if (PtInRect(&b.rect, pt)) {
                        newHover = b.id;
                        if (b.id == releasedBtnId) {
                            hitBtn = &b;
                        }
                        break;
                    }
                }
                g_Daemon.hoveredBtnId = newHover;
                if (hitBtn) {
                    DockButton copyBtn = *hitBtn;
                    ExecuteOverlayDockButtonAction(hWnd, copyBtn);
                    // Paint the result now instead of leaving it to the next idle
                    // moment: WM_PAINT is only synthesised when the queue is empty,
                    // and the 40 ms hover poll plus a moving mouse keep it busy.
                    if (IsWindow(hWnd)) UpdateWindow(hWnd);
                } else if (IsWindow(hWnd)) {
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                return 0;
            }
            if (g_Daemon.isDraggingStrokeText || g_Daemon.isDraggingSizeText || g_Daemon.isDraggingRatioText || g_Daemon.isDraggingTextBoxSel) {
                g_Daemon.isDraggingStrokeText = false;
                g_Daemon.isDraggingSizeText = false;
                g_Daemon.isDraggingRatioText = false;
                g_Daemon.isDraggingTextBoxSel = false;
                ReleaseCapture();
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (g_Daemon.dragMode != DragMode::None) {
                if (g_Daemon.dragMode == DragMode::DrawingAnnotation) {
                    g_Daemon.PushUndo();
                    if ((g_Daemon.draftAnn.tool == OverlayTool::Pen || g_Daemon.draftAnn.tool == OverlayTool::Highlighter) &&
                        !g_Daemon.draftAnn.points.empty()) {
                        float minX = g_Daemon.draftAnn.points[0].x, maxX = minX;
                        float minY = g_Daemon.draftAnn.points[0].y, maxY = minY;
                        for (const auto& p : g_Daemon.draftAnn.points) {
                            minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
                            minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
                        }
                        g_Daemon.draftAnn.startPt = { minX, minY };
                        g_Daemon.draftAnn.endPt   = { maxX, maxY };
                    }
                    g_Daemon.isPenShiftSnapping = false;
                    if (g_Daemon.draftAnn.tool == OverlayTool::NumberArrow) {
                        g_Daemon.nextStepNum++;
                    }
                    g_Daemon.annotations.push_back(g_Daemon.draftAnn);
                } else if (g_Daemon.dragMode == DragMode::CreatingSelection) {
                    int w = std::abs(g_Daemon.selRect.right - g_Daemon.selRect.left);
                    int h = std::abs(g_Daemon.selRect.bottom - g_Daemon.selRect.top);
                    if (w < 1 || h < 1) g_Daemon.hasSelection = false;
                } else if (g_Daemon.dragMode == DragMode::PendingOutsideSelection) {
                    int mx = GET_X_LPARAM(lParam);
                    int my = GET_Y_LPARAM(lParam);
                    g_Daemon.mousePt = { mx, my };
                    g_Daemon.dragMode = DragMode::None;
                    bool ctrlHeld = ((GetKeyState(VK_CONTROL) & 0x8000) != 0) ||
                                    ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) ||
                                    ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0) ||
                                    ((GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0);
                    if (!g_Daemon.hasSelection && g_Daemon.pendingShiftColorPick && !ctrlHeld) {
                        g_Daemon.pendingShiftColorPick = false;
                        g_Daemon.pendingCtrlWindowSelect = false;
                        ReleaseCapture();
                        g_Daemon.ActionCopyPixelColorAndClose();
                        return 0;
                    }
                    if (!g_Daemon.hasSelection && (ctrlHeld || g_Daemon.pendingCtrlWindowSelect)) {
                        if (ctrlHeld) {
                            g_Daemon.UpdateCtrlWindowHover();
                        }
                        RECT winRc = (ctrlHeld && g_Daemon.hasCtrlHoverWindow)
                            ? g_Daemon.ctrlHoverWindowRect
                            : g_Daemon.pendingCtrlWindowRect;
                        bool hasWin = (ctrlHeld && g_Daemon.hasCtrlHoverWindow) ||
                                      (g_Daemon.pendingCtrlWindowSelect &&
                                       (winRc.right > winRc.left) && (winRc.bottom > winRc.top));
                        if (hasWin) {
                            g_Daemon.hasSelection = true;
                            g_Daemon.hasEllipseSelection = false;
                            g_Daemon.selRect = winRc;
                            g_Daemon.SyncQuadFromSelRect();
                            g_Daemon.activeSelectionRatioW = 0;
                            g_Daemon.activeSelectionRatioH = 0;
                            g_Daemon.hasCtrlHoverWindow = false;
                            g_Daemon.hasCustomHudPos = false;
                            g_Daemon.activeTool = OverlayTool::SelectMove;
                            g_Daemon.annotations.clear();
                            g_Daemon.undoStack.clear();
                            g_Daemon.redoStack.clear();
                            g_Daemon.nextStepNum = 1;
                            g_Daemon.ClearAnnotationSelection();
                        }
                    }
                    g_Daemon.pendingShiftColorPick = false;
                    g_Daemon.pendingCtrlWindowSelect = false;
                    g_Daemon.UpdateOverlayCursor(mx, my);
                } else if (g_Daemon.dragMode == DragMode::MarqueeSelectingAnnotations) {
                    g_Daemon.hasMarqueeBox = false;
                } else if (g_Daemon.dragMode == DragMode::MovingAnnotation &&
                           g_Daemon.clickedAlreadySelectedAnn && !g_Daemon.moveAnnUndoPushed) {
                    g_Daemon.SelectSingleAnnotation(g_Daemon.clickedAlreadySelectedAnnId);
                    g_Daemon.clickedAlreadySelectedAnn = false;
                    g_Daemon.clickedAlreadySelectedAnnId = -1;
                } else if (g_Daemon.dragMode == DragMode::ResizingFrameStrip) {
                    g_Daemon.SaveSettings();
                }
                if (g_Daemon.isDraggingSingleCorner && g_Daemon.hasCustomQuad) {
                    g_Daemon.CanonicalizeSnappedQuad();
                    g_Daemon.SyncSelRectFromQuad();
                }
                g_Daemon.moveAnnUndoPushed = false;
                g_Daemon.clickedAlreadySelectedAnn = false;
                g_Daemon.activeSelectionRatioW = 0;
                g_Daemon.activeSelectionRatioH = 0;
                g_Daemon.isDraggingSingleCorner = false;
                g_Daemon.dragMode = DragMode::None;
                ReleaseCapture();
                if (g_Daemon.IsHoveringSelectedCustomArea(g_Daemon.mousePt)) {
                    g_Daemon.lastCustomAreaHoverMouse = g_Daemon.mousePt;
                    SetTimer(hWnd, 4, 500, nullptr);
                }
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_CHAR: {
            if (g_Daemon.isEditingStroke) {
                wchar_t ch = (wchar_t)wParam;
                if (ch == L'\r') {
                    g_Daemon.CommitActiveStrokeInput();
                } else if ((ch >= L'0' && ch <= L'9') || ch == L'.') {
                    PepperSnapDaemon::ApplyInlineEditChar(
                        ch, g_Daemon.editingStrokeText, g_Daemon.strokeCaretPos, g_Daemon.strokeSelAnchor, 5
                    );
                }
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (g_Daemon.isEditingSize) {
                wchar_t ch = (wchar_t)wParam;
                if (ch == L'\r') {
                    g_Daemon.CommitActiveSizeInput();
                } else if ((ch >= L'0' && ch <= L'9') || ch == L'x' || ch == L'X' || ch == L'*' || ch == L',' || ch == L' ') {
                    PepperSnapDaemon::ApplyInlineEditChar(
                        ch == L'X' ? L'x' : ch,
                        g_Daemon.editingSizeText, g_Daemon.sizeCaretPos, g_Daemon.sizeSelAnchor, 15
                    );
                }
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (g_Daemon.isEditingRatio) {
                wchar_t ch = (wchar_t)wParam;
                if (ch == L'\r') {
                    g_Daemon.CommitActiveRatioInput();
                } else if ((ch >= L'0' && ch <= L'9') || ch == L':' || ch == L'x' || ch == L'X' || ch == L'*' || ch == L',' || ch == L'/' || ch == L' ') {
                    wchar_t normCh = (ch == L'x' || ch == L'X' || ch == L'*' || ch == L',' || ch == L'/') ? L':' : ch;
                    PepperSnapDaemon::ApplyInlineEditChar(
                        normCh,
                        g_Daemon.editingRatioText, g_Daemon.ratioCaretPos, g_Daemon.ratioSelAnchor, 15
                    );
                }
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (g_Daemon.isEditingText) {
                wchar_t ch = (wchar_t)wParam;
                if (ch == L'\r') {
                    if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) {
                        PepperSnapDaemon::ApplyInlineEditChar(
                            L'\n', g_Daemon.editingTextAnn.text, g_Daemon.textCaretPos, g_Daemon.textSelAnchor, 4096
                        );
                    } else {
                        g_Daemon.CommitActiveTextBox();
                        g_Daemon.activeTool = OverlayTool::SelectMove;
                        g_Daemon.UpdateOverlayCursor(g_Daemon.mousePt.x, g_Daemon.mousePt.y);
                    }
                } else if (ch >= 32) {
                    PepperSnapDaemon::ApplyInlineEditChar(
                        ch, g_Daemon.editingTextAnn.text, g_Daemon.textCaretPos, g_Daemon.textSelAnchor, 4096
                    );
                }
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            break;
        }

        case WM_SYSKEYDOWN:
        case WM_KEYDOWN: {
            bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0 || (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0 || (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0 || (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
            if (g_Daemon.IsEditingOpenedImage()) {
                UINT capVk = (UINT)wParam;
                bool inlineEditActive = g_Daemon.isEditingText || g_Daemon.isEditingSize || g_Daemon.isEditingRatio || g_Daemon.isEditingStroke;
                if (MatchesOverlayShortcut(g_Daemon.hkPrevRegion, capVk, ctrl, shift, alt)) {
                    PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_PREV_REGION, 0, 0);
                    return 0;
                }
                if (MatchesOverlayShortcut(g_Daemon.hkRegionSnip, capVk, ctrl, shift, alt)) {
                    PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_REGION_SNIP, 0, 0);
                    return 0;
                }
                if (MatchesOverlayShortcut(g_Daemon.hkFullSnap, capVk, ctrl, shift, alt)) {
                    PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_FULL_SNAP, 0, 0);
                    return 0;
                }
                if (!inlineEditActive && MatchesOverlayShortcut(g_Daemon.hkPinClipboard, capVk, ctrl, shift, alt)) {
                    PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_PIN_CLIPBOARD, 0, 0);
                    return 0;
                }
                if (!inlineEditActive && MatchesOverlayShortcut(g_Daemon.hkEditClipboard, capVk, ctrl, shift, alt)) {
                    PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_EDIT_CLIPBOARD, 0, 0);
                    return 0;
                }
                if (!inlineEditActive && MatchesOverlayShortcut(g_Daemon.hkSearchGoogle, capVk, ctrl, shift, alt)) {
                    PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_SEARCH_GOOGLE, 0, 0);
                    return 0;
                }
            }

            if (g_Daemon.isEditingStroke) {
                if (wParam == VK_ESCAPE) {
                    g_Daemon.isEditingStroke = false;
                    g_Daemon.isDraggingStrokeText = false;
                    g_Daemon.editingStrokeText.clear();
                    g_Daemon.strokeCaretPos = g_Daemon.strokeSelAnchor = 0;
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
                if (wParam == VK_RETURN) {
                    g_Daemon.CommitActiveStrokeInput();
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
                PepperSnapDaemon::ApplyInlineEditKeyDown(
                    wParam, ctrl, shift,
                    g_Daemon.editingStrokeText, g_Daemon.strokeCaretPos, g_Daemon.strokeSelAnchor,
                    5, false, hWnd
                );
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }

            if (g_Daemon.isEditingSize) {
                if (wParam == VK_ESCAPE) {
                    g_Daemon.isEditingSize = false;
                    g_Daemon.isDraggingSizeText = false;
                    g_Daemon.editingSizeText.clear();
                    g_Daemon.sizeCaretPos = g_Daemon.sizeSelAnchor = 0;
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
                if (wParam == VK_RETURN) {
                    g_Daemon.CommitActiveSizeInput();
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
                PepperSnapDaemon::ApplyInlineEditKeyDown(
                    wParam, ctrl, shift,
                    g_Daemon.editingSizeText, g_Daemon.sizeCaretPos, g_Daemon.sizeSelAnchor,
                    15, true, hWnd
                );
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }

            if (g_Daemon.isEditingRatio) {
                if (wParam == VK_ESCAPE) {
                    g_Daemon.isEditingRatio = false;
                    g_Daemon.isDraggingRatioText = false;
                    g_Daemon.editingRatioText.clear();
                    g_Daemon.ratioCaretPos = g_Daemon.ratioSelAnchor = 0;
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
                if (wParam == VK_RETURN) {
                    g_Daemon.CommitActiveRatioInput();
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
                PepperSnapDaemon::ApplyInlineEditKeyDown(
                    wParam, ctrl, shift,
                    g_Daemon.editingRatioText, g_Daemon.ratioCaretPos, g_Daemon.ratioSelAnchor,
                    15, true, hWnd
                );
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }

            if (g_Daemon.isEditingText) {
                if (wParam == VK_ESCAPE) {
                    g_Daemon.CommitActiveTextBox();
                    g_Daemon.activeTool = OverlayTool::SelectMove;
                    g_Daemon.UpdateOverlayCursor(g_Daemon.mousePt.x, g_Daemon.mousePt.y);
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
                if (wParam == VK_RETURN && !shift) {
                    g_Daemon.CommitActiveTextBox();
                    g_Daemon.activeTool = OverlayTool::SelectMove;
                    g_Daemon.UpdateOverlayCursor(g_Daemon.mousePt.x, g_Daemon.mousePt.y);
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
                g_Daemon.ApplyTextBoxKeyDown(wParam, ctrl, shift, hWnd);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }

            // Pressing Esc when using any tool other than 'Select Mode' switches to 'Select Mode';
            //    only exit overlay when 'Select Mode' is already selected!
            if (wParam == VK_ESCAPE) {
                if (g_Daemon.activeTool != OverlayTool::SelectMove) {
                    g_Daemon.activeTool = OverlayTool::SelectMove;
                    g_Daemon.UpdateOverlayCursor(g_Daemon.mousePt.x, g_Daemon.mousePt.y);
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
                g_Daemon.CloseRegionSnipOverlay();
                return 0;
            }
            if ((wParam == VK_DELETE || wParam == VK_BACK) && g_Daemon.HasSelectedAnnotations()) {
                g_Daemon.PushUndo();
                g_Daemon.annotations.erase(
                    std::remove_if(g_Daemon.annotations.begin(), g_Daemon.annotations.end(),
                                   [](const Annotation& a) { return g_Daemon.IsAnnotationSelected(a.id); }),
                    g_Daemon.annotations.end()
                );
                g_Daemon.RecalcNextStepNum();
                g_Daemon.ClearAnnotationSelection();
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (ctrl && !shift && wParam == 'A' && g_Daemon.hasSelection && g_Daemon.GetEffectiveTool() == OverlayTool::SelectMove) {
                g_Daemon.selectedAnnotationIds.clear();
                g_Daemon.selectedAnnotationId = -1;
                for (const auto& a : g_Daemon.annotations) {
                    g_Daemon.selectedAnnotationIds.push_back(a.id);
                    g_Daemon.selectedAnnotationId = a.id;
                }
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (!g_Daemon.hasSelection && (ctrl || ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0)) && wParam == VK_RETURN) {
                g_Daemon.UpdateCtrlWindowHover();
                if (g_Daemon.hasCtrlHoverWindow) {
                    RECT winRc = g_Daemon.ctrlHoverWindowRect;
                    g_Daemon.hasSelection = true;
                    g_Daemon.hasEllipseSelection = false;
                    g_Daemon.selRect = winRc;
                    g_Daemon.SyncQuadFromSelRect();
                    g_Daemon.activeSelectionRatioW = 0;
                    g_Daemon.activeSelectionRatioH = 0;
                    g_Daemon.dragMode = DragMode::None;
                    g_Daemon.pendingCtrlWindowSelect = false;
                    g_Daemon.hasCtrlHoverWindow = false;
                    g_Daemon.hasCustomHudPos = false;
                    g_Daemon.activeTool = OverlayTool::SelectMove;
                    g_Daemon.annotations.clear();
                    g_Daemon.undoStack.clear();
                    g_Daemon.redoStack.clear();
                    g_Daemon.nextStepNum = 1;
                    g_Daemon.ClearAnnotationSelection();
                    g_Daemon.UpdateOverlayCursor(g_Daemon.mousePt.x, g_Daemon.mousePt.y);
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
            }
            if (wParam == VK_CONTROL) {
                if (!g_Daemon.hasSelection && (g_Daemon.dragMode == DragMode::None || g_Daemon.dragMode == DragMode::PendingOutsideSelection)) {
                    g_Daemon.lastCtrlTempSelect = false;
                    if (g_Daemon.UpdateCtrlWindowHover()) {
                        g_Daemon.UpdateOverlayCursor(g_Daemon.mousePt.x, g_Daemon.mousePt.y);
                        InvalidateRect(hWnd, nullptr, FALSE);
                    }
                } else if (g_Daemon.HasCreatedCustomArea()) {
                    bool curCtrlTempSelect = g_Daemon.IsCtrlTempSelectActive();
                    if (curCtrlTempSelect != g_Daemon.lastCtrlTempSelect) {
                        g_Daemon.lastCtrlTempSelect = curCtrlTempSelect;
                        g_Daemon.UpdateOverlayCursor(g_Daemon.mousePt.x, g_Daemon.mousePt.y);
                        InvalidateRect(hWnd, nullptr, FALSE);
                    }
                }
                return 0;
            }
            UINT vk = (UINT)wParam;
            bool isSelectionDragActive =
                (g_Daemon.dragMode == DragMode::PendingOutsideSelection ||
                 g_Daemon.dragMode == DragMode::CreatingSelection ||
                 g_Daemon.dragMode == DragMode::MovingSelection ||
                 g_Daemon.dragMode == DragMode::ResizeTL ||
                 g_Daemon.dragMode == DragMode::ResizeT  ||
                 g_Daemon.dragMode == DragMode::ResizeTR ||
                 g_Daemon.dragMode == DragMode::ResizeR  ||
                 g_Daemon.dragMode == DragMode::ResizeBR ||
                 g_Daemon.dragMode == DragMode::ResizeB  ||
                 g_Daemon.dragMode == DragMode::ResizeBL ||
                 g_Daemon.dragMode == DragMode::ResizeL);
            if ((!g_Daemon.hasSelection || isSelectionDragActive) && (!ctrl || g_Daemon.isDraggingSingleCorner) &&
                (wParam == VK_UP || wParam == VK_DOWN || wParam == VK_LEFT || wParam == VK_RIGHT)) {
                int stepX = (wParam == VK_LEFT) ? -1 : ((wParam == VK_RIGHT) ? 1 : 0);
                int stepY = (wParam == VK_UP)   ? -1 : ((wParam == VK_DOWN)  ? 1 : 0);
                int nextX = std::max(0, std::min(g_Daemon.vScreenW - 1, (int)g_Daemon.mousePt.x + stepX));
                int nextY = std::max(0, std::min(g_Daemon.vScreenH - 1, (int)g_Daemon.mousePt.y + stepY));

                if (g_Daemon.dragMode == DragMode::PendingOutsideSelection && !g_Daemon.pendingShiftColorPick) {
                    if (g_Daemon.autoHideFrameList && g_Daemon.HasOverlayMultiFrames() && !g_Daemon.overlayFrameStripCollapsed) {
                        g_Daemon.overlayFrameStripCollapsed = true;
                        g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                    }
                    g_Daemon.pendingShiftColorPick = false;
                    g_Daemon.pendingCtrlWindowSelect = false;
                    g_Daemon.hasSelection = true;
                    g_Daemon.hasEllipseSelection = g_Daemon.altHeldOnSelectionStart || alt;
                    g_Daemon.hasCustomHudPos = false;
                    g_Daemon.activeTool = OverlayTool::SelectMove;
                    g_Daemon.annotations.clear();
                    g_Daemon.undoStack.clear();
                    g_Daemon.redoStack.clear();
                    g_Daemon.nextStepNum = 1;
                    g_Daemon.ClearAnnotationSelection();
                    g_Daemon.dragMode = DragMode::CreatingSelection;
                    int startX = std::max(0, std::min(g_Daemon.vScreenW, (int)g_Daemon.dragStartPt.x));
                    int startY = std::max(0, std::min(g_Daemon.vScreenH, (int)g_Daemon.dragStartPt.y));
                    g_Daemon.dragOrigRect = { startX, startY, startX, startY };
                }

                int ratioW = 0, ratioH = 0;
                WPARAM curMouseWParam = 0;
                if ((GetKeyState(VK_LBUTTON) & 0x8000) != 0) curMouseWParam |= MK_LBUTTON;
                if (g_Daemon.isRightMouseHeld || (GetKeyState(VK_RBUTTON) & 0x8000) != 0) curMouseWParam |= MK_RBUTTON;
                if (shift) curMouseWParam |= MK_SHIFT;
                bool hasRatio = !g_Daemon.isDraggingSingleCorner &&
                                g_Daemon.GetActiveSelectionAspectRatio(curMouseWParam, ratioW, ratioH);
                if (hasRatio &&
                    (g_Daemon.dragMode == DragMode::CreatingSelection ||
                     g_Daemon.dragMode == DragMode::ResizeTL ||
                     g_Daemon.dragMode == DragMode::ResizeTR ||
                     g_Daemon.dragMode == DragMode::ResizeBR ||
                     g_Daemon.dragMode == DragMode::ResizeBL)) {
                    int anchorX = std::max(0, std::min(g_Daemon.vScreenW, (int)g_Daemon.dragStartPt.x));
                    int anchorY = std::max(0, std::min(g_Daemon.vScreenH, (int)g_Daemon.dragStartPt.y));
                    if (g_Daemon.dragMode == DragMode::ResizeTL) {
                        anchorX = g_Daemon.dragOrigRect.right;
                        anchorY = g_Daemon.dragOrigRect.bottom;
                    } else if (g_Daemon.dragMode == DragMode::ResizeTR) {
                        anchorX = g_Daemon.dragOrigRect.left;
                        anchorY = g_Daemon.dragOrigRect.bottom;
                    } else if (g_Daemon.dragMode == DragMode::ResizeBR) {
                        anchorX = g_Daemon.dragOrigRect.left;
                        anchorY = g_Daemon.dragOrigRect.top;
                    } else if (g_Daemon.dragMode == DragMode::ResizeBL) {
                        anchorX = g_Daemon.dragOrigRect.right;
                        anchorY = g_Daemon.dragOrigRect.top;
                    }
                    int curSdx = g_Daemon.selectionTargetPt.x - anchorX;
                    int curSdy = g_Daemon.selectionTargetPt.y - anchorY;
                    int curW = std::abs(curSdx);
                    int curH = std::abs(curSdy);
                    int dirX = (curSdx >= 0) ? 1 : -1;
                    int dirY = (curSdy >= 0) ? 1 : -1;
                    if (curW == 0 && curH == 0) {
                        if (stepX != 0) dirX = stepX;
                        if (stepY != 0) dirY = stepY;
                    }
                    int nextW = curW;
                    int nextH = curH;
                    if (stepX != 0) {
                        nextW = std::max(1, curW + stepX * dirX);
                        nextH = std::max(1, (int)std::round((double)nextW * (double)ratioH / (double)ratioW));
                    } else {
                        nextH = std::max(1, curH + stepY * dirY);
                        nextW = std::max(1, (int)std::round((double)nextH * (double)ratioW / (double)ratioH));
                    }
                    int maxW = (dirX >= 0) ? (g_Daemon.vScreenW - anchorX) : anchorX;
                    int maxH = (dirY >= 0) ? (g_Daemon.vScreenH - anchorY) : anchorY;
                    if (nextW > maxW) {
                        nextW = maxW;
                        nextH = std::max(1, (int)std::round((double)nextW * (double)ratioH / (double)ratioW));
                    }
                    if (nextH > maxH) {
                        nextH = maxH;
                        nextW = std::max(1, (int)std::round((double)nextH * (double)ratioW / (double)ratioH));
                    }
                    nextW = std::min(nextW, maxW);
                    nextH = std::min(nextH, maxH);
                    nextX = std::max(0, std::min(g_Daemon.vScreenW - 1, anchorX + dirX * nextW));
                    nextY = std::max(0, std::min(g_Daemon.vScreenH - 1, anchorY + dirY * nextH));
                }

                g_Daemon.mousePt.x = nextX;
                g_Daemon.mousePt.y = nextY;
                SetCursorPos(g_Daemon.vScreenX + nextX, g_Daemon.vScreenY + nextY);

                WPARAM moveWParam = 0;
                if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0) moveWParam |= MK_LBUTTON;
                if ((GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0 || g_Daemon.isRightMouseHeld) moveWParam |= MK_RBUTTON;
                if (shift) moveWParam |= MK_SHIFT;
                SendMessageW(hWnd, WM_MOUSEMOVE, moveWParam, MAKELPARAM(nextX, nextY));
                g_Daemon.UpdateOverlayCursor(nextX, nextY);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }

            if (isSelectionDragActive &&
                (wParam == VK_SHIFT || wParam == VK_LSHIFT || wParam == VK_RSHIFT ||
                 wParam == VK_MENU  || wParam == VK_LMENU  || wParam == VK_RMENU  ||
                 wParam == '1' || wParam == '2' || wParam == '3' || wParam == '4' ||
                 wParam == VK_NUMPAD1 || wParam == VK_NUMPAD2 || wParam == VK_NUMPAD3 || wParam == VK_NUMPAD4)) {
                WPARAM moveWParam = MK_LBUTTON;
                if (shift || wParam == VK_SHIFT || wParam == VK_LSHIFT || wParam == VK_RSHIFT) moveWParam |= MK_SHIFT;
                if (g_Daemon.isRightMouseHeld || (GetKeyState(VK_RBUTTON) & 0x8000) != 0) moveWParam |= MK_RBUTTON;
                SendMessageW(hWnd, WM_MOUSEMOVE, moveWParam, MAKELPARAM(g_Daemon.mousePt.x, g_Daemon.mousePt.y));
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (wParam == VK_MENU || wParam == VK_LMENU || wParam == VK_RMENU) {
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }

            if (g_Daemon.HasOverlayMultiFrames()) {
                if (MatchesOverlayShortcut(g_Daemon.hkFramePlayPause, vk, ctrl, shift, alt)) {
                    g_Daemon.ToggleOverlayFramePlayback();
                    return 0;
                }
                if (MatchesOverlayShortcut(g_Daemon.hkFrameToggleStrip, vk, ctrl, shift, alt)) {
                    g_Daemon.overlayFrameStripCollapsed = !g_Daemon.overlayFrameStripCollapsed;
                    if (!g_Daemon.overlayFrameStripCollapsed) {
                        g_Daemon.EnsureActiveFrameVisibleInStrip();
                    }
                    g_Daemon.LayoutOverlayFrameStrip(g_Daemon.vScreenW, g_Daemon.vScreenH);
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
            }

            if (MatchesOverlayShortcut(g_Daemon.hkActSearchGoogle, vk, ctrl, shift, alt)) {
                if (g_Daemon.hasSelection &&
                    g_Daemon.dragMode != DragMode::CreatingSelection &&
                    g_Daemon.dragMode != DragMode::PendingOutsideSelection) {
                    g_Daemon.ActionCopyAndClose(WebOpenTarget::Lens);
                }
                return 0;
            }
            if (MatchesOverlayShortcut(g_Daemon.hkActCopy, vk, ctrl, shift, alt)) {
                if (g_Daemon.hasSelection &&
                    g_Daemon.dragMode != DragMode::CreatingSelection &&
                    g_Daemon.dragMode != DragMode::PendingOutsideSelection) {
                    g_Daemon.ActionCopyAndClose(WebOpenTarget::None);
                }
                return 0;
            }
            if (MatchesOverlayShortcut(g_Daemon.hkActSaveAs, vk, ctrl, shift, alt)) {
                if (g_Daemon.hasSelection) {
                    g_Daemon.ActionSaveAsAndClose();
                }
                return 0;
            }
            if (MatchesOverlayShortcut(g_Daemon.hkActSave, vk, ctrl, shift, alt)) {
                g_Daemon.ActionQuickSaveAndClose();
                return 0;
            }
            if (MatchesOverlayShortcut(g_Daemon.hkActUndo, vk, ctrl, shift, alt)) {
                g_Daemon.Undo();
                return 0;
            }
            if (MatchesOverlayShortcut(g_Daemon.hkActRedo, vk, ctrl, shift, alt)) {
                g_Daemon.Redo();
                return 0;
            }
            if (MatchesOverlayShortcut(g_Daemon.hkActClear, vk, ctrl, shift, alt)) {
                if (g_Daemon.hasSelection) {
                    g_Daemon.PushUndo();
                    g_Daemon.annotations.clear();
                    g_Daemon.nextStepNum = 1;
                    g_Daemon.ClearAnnotationSelection();
                    g_Daemon.BuildDockedHUD();
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                return 0;
            }
            if (MatchesOverlayShortcut(g_Daemon.hkActPin, vk, ctrl, shift, alt)) {
                g_Daemon.ActionPinToDesktop();
                return 0;
            }
            if (MatchesOverlayShortcut(g_Daemon.hkActOcr, vk, ctrl, shift, alt)) {
                g_Daemon.ActionOcrAndClose();
                return 0;
            }
            if (MatchesOverlayShortcut(g_Daemon.hkToolResetNum, vk, ctrl, shift, alt)) {
                if (g_Daemon.hasSelection) {
                    g_Daemon.nextStepNum = 1;
                    g_Daemon.BuildDockedHUD();
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                return 0;
            }

            if (MatchesOverlayShortcut(g_Daemon.hkToolSelect, vk, ctrl, shift, alt))         g_Daemon.activeTool = OverlayTool::SelectMove;
            else if (MatchesOverlayShortcut(g_Daemon.hkToolPen, vk, ctrl, shift, alt))       g_Daemon.activeTool = OverlayTool::Pen;
            else if (MatchesOverlayShortcut(g_Daemon.hkToolStabilo, vk, ctrl, shift, alt))   g_Daemon.activeTool = OverlayTool::Highlighter;
            else if (MatchesOverlayShortcut(g_Daemon.hkToolLine, vk, ctrl, shift, alt))      g_Daemon.activeTool = OverlayTool::Line;
            else if (MatchesOverlayShortcut(g_Daemon.hkToolArrow, vk, ctrl, shift, alt))     g_Daemon.activeTool = OverlayTool::Arrow;
            else if (MatchesOverlayShortcut(g_Daemon.hkToolNumber, vk, ctrl, shift, alt))    g_Daemon.activeTool = OverlayTool::NumberArrow;
            else if (MatchesOverlayShortcut(g_Daemon.hkToolRect, vk, ctrl, shift, alt))      g_Daemon.activeTool = OverlayTool::Rectangle;
            else if (MatchesOverlayShortcut(g_Daemon.hkToolEllipse, vk, ctrl, shift, alt))   g_Daemon.activeTool = OverlayTool::Ellipse;
            else if (MatchesOverlayShortcut(g_Daemon.hkToolText, vk, ctrl, shift, alt))      g_Daemon.activeTool = OverlayTool::TextBox;
            else if (MatchesOverlayShortcut(g_Daemon.hkToolMosaicSq, vk, ctrl, shift, alt))  g_Daemon.activeTool = OverlayTool::MosaicSquare;
            else if (MatchesOverlayShortcut(g_Daemon.hkToolMosaicCir, vk, ctrl, shift, alt)) g_Daemon.activeTool = OverlayTool::MosaicCircle;

            g_Daemon.UpdateOverlayCursor(g_Daemon.mousePt.x, g_Daemon.mousePt.y);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        case WM_SYSKEYUP:
        case WM_KEYUP: {
            if (wParam == VK_SNAPSHOT && g_Daemon.IsEditingOpenedImage()) {
                bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0 || (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0 || (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0 || (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
                if (MatchesOverlayShortcut(g_Daemon.hkPrevRegion, VK_SNAPSHOT, ctrl, shift, alt)) {
                    PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_PREV_REGION, 0, 0);
                    return 0;
                }
                if (MatchesOverlayShortcut(g_Daemon.hkRegionSnip, VK_SNAPSHOT, ctrl, shift, alt)) {
                    PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_REGION_SNIP, 0, 0);
                    return 0;
                }
                if (MatchesOverlayShortcut(g_Daemon.hkFullSnap, VK_SNAPSHOT, ctrl, shift, alt)) {
                    PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_FULL_SNAP, 0, 0);
                    return 0;
                }
                if (MatchesOverlayShortcut(g_Daemon.hkEditClipboard, VK_SNAPSHOT, ctrl, shift, alt)) {
                    PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_EDIT_CLIPBOARD, 0, 0);
                    return 0;
                }
                if (MatchesOverlayShortcut(g_Daemon.hkSearchGoogle, VK_SNAPSHOT, ctrl, shift, alt)) {
                    PostMessageW(g_Daemon.hTrayWnd, WM_TRIGGER_SEARCH_GOOGLE, 0, 0);
                    return 0;
                }
            }
            if (wParam == VK_SHIFT || wParam == VK_LSHIFT || wParam == VK_RSHIFT) {
                g_Daemon.lastShiftColorPick = false;
                bool isSelectionOrResizeDrag =
                    (g_Daemon.dragMode == DragMode::PendingOutsideSelection ||
                     g_Daemon.dragMode == DragMode::CreatingSelection ||
                     (g_Daemon.dragMode >= DragMode::ResizeTL && g_Daemon.dragMode <= DragMode::ResizeL));
                if (isSelectionOrResizeDrag) {
                    WPARAM moveWParam = MK_LBUTTON;
                    if (g_Daemon.isRightMouseHeld || (GetKeyState(VK_RBUTTON) & 0x8000) != 0) moveWParam |= MK_RBUTTON;
                    SendMessageW(hWnd, WM_MOUSEMOVE, moveWParam, MAKELPARAM(g_Daemon.mousePt.x, g_Daemon.mousePt.y));
                    InvalidateRect(hWnd, nullptr, FALSE);
                } else if (!g_Daemon.hasSelection) {
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                return 0;
            }
            if (wParam == VK_MENU || wParam == VK_LMENU || wParam == VK_RMENU) {
                bool isSelectionOrResizeDrag =
                    (g_Daemon.dragMode == DragMode::PendingOutsideSelection ||
                     g_Daemon.dragMode == DragMode::CreatingSelection ||
                     (g_Daemon.dragMode >= DragMode::ResizeTL && g_Daemon.dragMode <= DragMode::ResizeL));
                if (isSelectionOrResizeDrag) {
                    WPARAM moveWParam = MK_LBUTTON;
                    if ((GetKeyState(VK_SHIFT) & 0x8000) != 0 || (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) moveWParam |= MK_SHIFT;
                    if (g_Daemon.isRightMouseHeld || (GetKeyState(VK_RBUTTON) & 0x8000) != 0) moveWParam |= MK_RBUTTON;
                    SendMessageW(hWnd, WM_MOUSEMOVE, moveWParam, MAKELPARAM(g_Daemon.mousePt.x, g_Daemon.mousePt.y));
                    InvalidateRect(hWnd, nullptr, FALSE);
                } else if (!g_Daemon.hasSelection) {
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                return 0;
            }
            if (wParam == VK_CONTROL) {
                bool changed = g_Daemon.UpdateCtrlWindowHover();
                if (g_Daemon.HasCreatedCustomArea()) {
                    bool curCtrlTempSelect = g_Daemon.IsCtrlTempSelectActive();
                    if (curCtrlTempSelect != g_Daemon.lastCtrlTempSelect) {
                        g_Daemon.lastCtrlTempSelect = curCtrlTempSelect;
                        changed = true;
                    }
                } else {
                    g_Daemon.lastCtrlTempSelect = false;
                }
                if (changed) {
                    g_Daemon.UpdateOverlayCursor(g_Daemon.mousePt.x, g_Daemon.mousePt.y);
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                return 0;
            }
            if (wParam == '1' || wParam == '2' || wParam == '3' || wParam == '4' ||
                wParam == VK_NUMPAD1 || wParam == VK_NUMPAD2 || wParam == VK_NUMPAD3 || wParam == VK_NUMPAD4) {
                bool isSelectionOrResizeDrag =
                    (g_Daemon.dragMode == DragMode::PendingOutsideSelection ||
                     g_Daemon.dragMode == DragMode::CreatingSelection ||
                     (g_Daemon.dragMode >= DragMode::ResizeTL && g_Daemon.dragMode <= DragMode::ResizeL));
                if (isSelectionOrResizeDrag) {
                    WPARAM moveWParam = MK_LBUTTON;
                    if (g_Daemon.isRightMouseHeld || (GetKeyState(VK_RBUTTON) & 0x8000) != 0) moveWParam |= MK_RBUTTON;
                    SendMessageW(hWnd, WM_MOUSEMOVE, moveWParam, MAKELPARAM(g_Daemon.mousePt.x, g_Daemon.mousePt.y));
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                return 0;
            }
            break;
        }

        case WM_DESTROY:
            KillTimer(hWnd, 1);
            KillTimer(hWnd, 2);
            KillTimer(hWnd, 3);
            KillTimer(hWnd, 4);
            g_Daemon.showingCustomAreaHoverTip = false;
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

struct EnumDesktopWindowsCtx {
    PepperSnapDaemon* daemon;
    std::vector<RECT>* rects;
};

struct CandidateWindowEntry {
    HWND hwnd = nullptr;
    int layerTier = 2; // 0 = frontmost notifications/sidebars/flyouts/menus, 1 = topmost/taskbars, 2 = standard windows
    int zOrderIdx = 0;
};

static bool IsSameOrNearlySameRect(const RECT& a, const RECT& b, LONG tol = 3) {
    return std::abs(a.left - b.left) <= tol &&
           std::abs(a.top - b.top) <= tol &&
           std::abs(a.right - b.right) <= tol &&
           std::abs(a.bottom - b.bottom) <= tol;
}

static void PushUniqueWindowRect(std::vector<RECT>& outRects, const RECT& rc) {
    if (rc.right - rc.left < 16 || rc.bottom - rc.top < 16) return;
    for (const auto& existing : outRects) {
        if (IsSameOrNearlySameRect(existing, rc, 3)) {
            return;
        }
    }
    outRects.push_back(rc);
}

static RECT ClampScreenRectToOverlay(const PepperSnapDaemon* d, RECT rc) {
    rc.left   -= d->vScreenX;
    rc.top    -= d->vScreenY;
    rc.right  -= d->vScreenX;
    rc.bottom -= d->vScreenY;

    rc.left   = std::max(0L, std::min((LONG)d->vScreenW, rc.left));
    rc.top    = std::max(0L, std::min((LONG)d->vScreenH, rc.top));
    rc.right  = std::max(0L, std::min((LONG)d->vScreenW, rc.right));
    rc.bottom = std::max(0L, std::min((LONG)d->vScreenH, rc.bottom));
    return rc;
}

// Extract visible sub-panels (such as Windows 11 Notification Center cards, Calendar flyout,
// Toast notification popups, Emoji/Clipboard popups, etc.) from large CoreWindow / XAML Island hosts
static void ExtractUiaSubWindowRects(
    IUIAutomation* pUia,
    IUIAutomationTreeWalker* pWalker,
    HWND hwnd,
    const RECT& hostOverlayRc,
    const PepperSnapDaemon* d,
    bool isFullscreenHost,
    std::vector<RECT>& outSubRects
) {
    if (!pUia || !pWalker || !hwnd) return;
    IUIAutomationElement* pRoot = nullptr;
    if (FAILED(pUia->ElementFromHandle((UIA_HWND)hwnd, &pRoot)) || !pRoot) return;

    LONG hostW = hostOverlayRc.right - hostOverlayRc.left;
    LONG hostH = hostOverlayRc.bottom - hostOverlayRc.top;
    long long hostArea = (long long)hostW * (long long)hostH;

    auto collectLevelPanels = [&](IUIAutomationElement* parentElem, auto& selfRef, int depth) -> void {
        if (!parentElem || depth > 3) return;
        IUIAutomationElement* pChild = nullptr;
        if (FAILED(pWalker->GetFirstChildElement(parentElem, &pChild)) || !pChild) return;

        while (pChild) {
            WINBOOL isOffscreen = FALSE;
            RECT childScreenRc = {0, 0, 0, 0};
            CONTROLTYPEID ctrlType = 0;
            bool okBounds = SUCCEEDED(pChild->get_CurrentBoundingRectangle(&childScreenRc));
            pChild->get_CurrentIsOffscreen(&isOffscreen);
            pChild->get_CurrentControlType(&ctrlType);

            if (okBounds && !isOffscreen) {
                RECT cRc = ClampScreenRectToOverlay(d, childScreenRc);
                LONG cw = cRc.right - cRc.left;
                LONG ch = cRc.bottom - cRc.top;
                long long cArea = (long long)cw * (long long)ch;

                if (cw >= 48 && ch >= 28) {
                    // If this child is a full-size wrapper around the host (>= 92% of host area), unwrap it
                    if (hostArea > 0 && cArea >= (hostArea * 92) / 100) {
                        selfRef(pChild, selfRef, depth + 1);
                    } else {
                        bool isContainerType =
                            ctrlType == UIA_WindowControlTypeId ||
                            ctrlType == UIA_PaneControlTypeId ||
                            ctrlType == UIA_GroupControlTypeId ||
                            ctrlType == UIA_CustomControlTypeId ||
                            ctrlType == UIA_MenuControlTypeId ||
                            ctrlType == UIA_ToolTipControlTypeId ||
                            ctrlType == UIA_ToolBarControlTypeId ||
                            ctrlType == UIA_ListControlTypeId;

                        bool validPanelSize = isFullscreenHost
                            ? (cw >= 80 && ch >= 32 && cArea < (hostArea * 85) / 100)
                            : (cw >= (hostW * 65) / 100 && ch >= 40 && ch < (hostH * 94) / 100);

                        if (isContainerType && validPanelSize) {
                            PushUniqueWindowRect(outSubRects, cRc);
                        } else if (depth < 2 && cArea >= (hostArea * 25) / 100) {
                            selfRef(pChild, selfRef, depth + 1);
                        }
                    }
                }
            }

            IUIAutomationElement* pNext = nullptr;
            if (FAILED(pWalker->GetNextSiblingElement(pChild, &pNext))) {
                pNext = nullptr;
            }
            pChild->Release();
            pChild = pNext;
        }
    };

    collectLevelPanels(pRoot, collectLevelPanels, 1);
    pRoot->Release();
}

static BOOL CALLBACK CollectEnumWindowsHwndProc(HWND hwnd, LPARAM lParam) {
    std::vector<HWND>* list = (std::vector<HWND>*)lParam;
    if (!list || !hwnd) return TRUE;
    if (std::find(list->begin(), list->end(), hwnd) == list->end()) {
        list->push_back(hwnd);
    }
    return TRUE;
}

static BOOL CALLBACK EnumMonitorWorkAreaProc(HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam) {
    EnumDesktopWindowsCtx* ctx = (EnumDesktopWindowsCtx*)lParam;
    if (!ctx || !ctx->daemon || !ctx->rects) return TRUE;
    PepperSnapDaemon* d = ctx->daemon;

    MONITORINFO mi = { sizeof(MONITORINFO) };
    if (!GetMonitorInfoW(hMonitor, &mi)) return TRUE;

    // mi.rcWork is the monitor's desktop area excluding the Windows taskbar
    RECT rc = mi.rcWork;
    HWND hTaskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (hTaskbar && IsWindowVisible(hTaskbar)) {
        RECT tbRc = {0, 0, 0, 0};
        if (GetWindowRect(hTaskbar, &tbRc)) {
            SubtractRect(&rc, &rc, &tbRc);
        }
    }
    HWND hSecTaskbar = nullptr;
    while ((hSecTaskbar = FindWindowExW(nullptr, hSecTaskbar, L"Shell_SecondaryTrayWnd", nullptr)) != nullptr) {
        if (IsWindowVisible(hSecTaskbar)) {
            RECT tbRc = {0, 0, 0, 0};
            if (GetWindowRect(hSecTaskbar, &tbRc)) {
                SubtractRect(&rc, &rc, &tbRc);
            }
        }
    }

    rc = ClampScreenRectToOverlay(d, rc);
    PushUniqueWindowRect(*ctx->rects, rc);
    return TRUE;
}

void PepperSnapDaemon::SnapshotDesktopWindows() {
    desktopWindowRects.clear();
    hasCtrlHoverWindow = false;
    ctrlHoverWindowRect = {0, 0, 0, 0};

    // Collect all top-level windows across ALL Z-order bands (ZBID_DESKTOP, ZBID_IMMERSIVE_NOTIFICATIONS,
    //    ZBID_IMMERSIVE_MOGO, ZBID_IMMERSIVE_SEARCH, ZBID_SYSTEM_TOOLS, ZBID_ABOVELOCK_UX, etc.)
    std::vector<HWND> rawHwnds;
    rawHwnds.reserve(256);

    // FindWindowExW(nullptr, prev, nullptr, nullptr) traverses the desktop's global child list across all ZBID bands
    HWND hCur = nullptr;
    for (int iter = 0; iter < 4096; ++iter) {
        hCur = FindWindowExW(nullptr, hCur, nullptr, nullptr);
        if (!hCur) break;
        if (std::find(rawHwnds.begin(), rawHwnds.end(), hCur) == rawHwnds.end()) {
            rawHwnds.push_back(hCur);
        }
    }

    // Also run EnumWindows & EnumDesktopWindows to ensure no desktop window is missed
    EnumWindows(CollectEnumWindowsHwndProc, (LPARAM)&rawHwnds);
    EnumDesktopWindows(GetThreadDesktop(GetCurrentThreadId()), CollectEnumWindowsHwndProc, (LPARAM)&rawHwnds);

    typedef BOOL (WINAPI *PFN_GetWindowBand)(HWND, PDWORD);
    static PFN_GetWindowBand s_fnGetWindowBand = nullptr;
    static bool s_resolvedBandFn = false;
    if (!s_resolvedBandFn) {
        HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
        if (hUser32) {
            s_fnGetWindowBand = (PFN_GetWindowBand)GetProcAddress(hUser32, "GetWindowBand");
        }
        s_resolvedBandFn = true;
    }

    std::vector<CandidateWindowEntry> candidates;
    candidates.reserve(rawHwnds.size());

    for (size_t i = 0; i < rawHwnds.size(); ++i) {
        HWND hwnd = rawHwnds[i];
        if (!hwnd || !IsWindow(hwnd) || !IsWindowVisible(hwnd) || IsIconic(hwnd)) continue;
        if (hwnd == hOverlayWnd || hwnd == hTrayWnd) continue;

        HWND hRootOwner = GetAncestor(hwnd, GA_ROOTOWNER);
        if (hRootOwner && hRootOwner != hwnd && IsIconic(hRootOwner)) continue;

        // Skip cloaked Windows 10/11 UWP / background shell windows (DWMWA_CLOAKED = 14)
        DWORD cloaked = 0;
        if (SUCCEEDED(DwmGetWindowAttribute(hwnd, 14, &cloaked, sizeof(cloaked))) && cloaked != 0) {
            continue;
        }

        LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        if (exStyle & WS_EX_LAYERED) {
            BYTE bAlpha = 255;
            DWORD dwFlags = 0;
            if (GetLayeredWindowAttributes(hwnd, nullptr, &bAlpha, &dwFlags) &&
                (dwFlags & LWA_ALPHA) && bAlpha < 15) {
                continue;
            }
        }

        WCHAR clsName[128] = {0};
        GetClassNameW(hwnd, clsName, 127);
        // PepperSnap's own top-level windows are deliberately absent from this
        // skip list, except for the three below. Whatever is visible in the
        // frozen screenshot has to stay selectable with Ctrl + hover, and that
        // includes the Options window, the shortcuts window and their sub-windows:
        // they sit behind the overlay but they are in the bitmap. Filtering only
        // Options and the shortcuts window (this list used to) meant Ctrl + hover
        // over them highlighted whatever window was drawn behind, while their own
        // sub-windows - Customize keys and the naming-syntax list - highlighted
        // correctly.
        if (wcscmp(clsName, L"Progman") == 0 ||
            wcscmp(clsName, L"WorkerW") == 0 ||
            wcscmp(clsName, L"DummyDWMListenerWindow") == 0 ||
            wcscmp(clsName, L"EdgeUiInputTopWndClass") == 0 ||
            wcscmp(clsName, L"EdgeUiInputWndClass") == 0 ||
            wcscmp(clsName, L"IME") == 0 ||
            wcscmp(clsName, L"Default IME") == 0 ||
            wcscmp(clsName, L"MSCTFIME UI") == 0 ||
            wcscmp(clsName, L"SysShadow") == 0 ||
            wcscmp(clsName, L"PepperSnapOverlayWnd") == 0 ||
            wcscmp(clsName, L"PepperSnapBubbleTipWnd") == 0 ||
            wcscmp(clsName, L"PepperSnapTrayDaemonClass") == 0) {
            continue;
        }

        DWORD band = 1; // ZBID_DESKTOP = 1
        if (s_fnGetWindowBand) {
            s_fnGetWindowBand(hwnd, &band);
        }

        bool isTaskbar = (wcscmp(clsName, L"Shell_TrayWnd") == 0 ||
                          wcscmp(clsName, L"Shell_SecondaryTrayWnd") == 0);
        bool isCoreOrIsland = (wcscmp(clsName, L"Windows.UI.Core.CoreWindow") == 0 ||
                               wcscmp(clsName, L"XamlExplorerHostIslandWindow") == 0 ||
                               wcscmp(clsName, L"ControlCenterWindow") == 0 ||
                               wcscmp(clsName, L"TopLevelWindowForOverflowXamlIsland") == 0 ||
                               wcscmp(clsName, L"Xaml_WindowedPopupClass") == 0 ||
                               wcscmp(clsName, L"PopupHost") == 0 ||
                               wcscmp(clsName, L"NotifyIconOverflowWindow") == 0 ||
                               wcscmp(clsName, L"tooltips_class32") == 0 ||
                               wcscmp(clsName, L"#32768") == 0);

        int tier = 2;
        if (!isTaskbar && (isCoreOrIsland ||
            band == 2  /* ZBID_UIACCESS */ ||
            band == 3  /* ZBID_IMMERSIVE_IHM */ ||
            band == 4  /* ZBID_IMMERSIVE_NOTIFICATIONS */ ||
            band == 6  /* ZBID_IMMERSIVE_MOGO */ ||
            band == 13 /* ZBID_IMMERSIVE_SEARCH */ ||
            band == 16 /* ZBID_SYSTEM_TOOLS */ ||
            band == 18 /* ZBID_ABOVELOCK_UX */)) {
            tier = 0; // Frontmost notifications, notification sidebar, flyouts, popups, menus
        } else if (isTaskbar || (exStyle & WS_EX_TOPMOST) != 0) {
            tier = 1; // Topmost windows & taskbars
        }

        candidates.push_back({ hwnd, tier, (int)i });
    }

    std::stable_sort(candidates.begin(), candidates.end(), [](const CandidateWindowEntry& a, const CandidateWindowEntry& b) {
        if (a.layerTier != b.layerTier) return a.layerTier < b.layerTier;
        return a.zOrderIdx < b.zOrderIdx;
    });

    // Lazily initialize UI Automation only if we encounter a large CoreWindow / XAML Island host
    IUIAutomation* pUia = nullptr;
    IUIAutomationTreeWalker* pWalker = nullptr;
    bool triedUiaInit = false;

    auto ensureUia = [&]() -> bool {
        if (!triedUiaInit) {
            triedUiaInit = true;
            static const GUID CLSID_CUIAutomation_Local = { 0xff48dba4, 0x60ef, 0x4201, { 0xaa, 0x87, 0x54, 0x10, 0x3e, 0xef, 0x59, 0x4e } };
            static const GUID IID_IUIAutomation_Local   = { 0x30cbe57d, 0xd9d0, 0x452a, { 0xab, 0x13, 0x7a, 0xc5, 0xac, 0x48, 0x25, 0xee } };
            if (SUCCEEDED(CoCreateInstance(CLSID_CUIAutomation_Local, nullptr, CLSCTX_INPROC_SERVER, IID_IUIAutomation_Local, (void**)&pUia)) && pUia) {
                pUia->get_ControlViewWalker(&pWalker);
            }
        }
        return (pUia != nullptr && pWalker != nullptr);
    };

    for (const auto& cand : candidates) {
        HWND hwnd = cand.hwnd;
        WCHAR clsName[128] = {0};
        GetClassNameW(hwnd, clsName, 127);
        WCHAR winTitle[256] = {0};
        GetWindowTextW(hwnd, winTitle, 255);

        RECT rawRc = {0, 0, 0, 0};
        if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rawRc, sizeof(rawRc))) ||
            (rawRc.right - rawRc.left) <= 0 || (rawRc.bottom - rawRc.top) <= 0) {
            if (!GetWindowRect(hwnd, &rawRc)) continue;
        }

        RECT rc = ClampScreenRectToOverlay(this, rawRc);
        LONG w = rc.right - rc.left;
        LONG h = rc.bottom - rc.top;
        if (w < 16 || h < 16) continue;

        HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(MONITORINFO) };
        LONG monW = vScreenW, monH = vScreenH;
        if (hMon && GetMonitorInfoW(hMon, &mi)) {
            monW = std::max(1L, mi.rcMonitor.right - mi.rcMonitor.left);
            monH = std::max(1L, mi.rcMonitor.bottom - mi.rcMonitor.top);
        }

        LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        bool isFullscreenMon = (w >= (monW * 85) / 100 && h >= (monH * 85) / 100);
        bool isTallColumn    = (h >= (monH * 75) / 100);

        // Skip fullscreen click-through transparent overlays (while keeping non-fullscreen notification balloons/popups!)
        if ((exStyle & WS_EX_TRANSPARENT) && isFullscreenMon) {
            continue;
        }

        // Skip untitled fullscreen background host windows on the desktop
        if (w >= vScreenW && h >= vScreenH && winTitle[0] == L'\0' &&
            wcscmp(clsName, L"Windows.UI.Core.CoreWindow") != 0 &&
            wcscmp(clsName, L"XamlExplorerHostIslandWindow") != 0) {
            continue;
        }

        bool isCoreOrXamlHost = (wcscmp(clsName, L"Windows.UI.Core.CoreWindow") == 0 ||
                                 wcscmp(clsName, L"XamlExplorerHostIslandWindow") == 0 ||
                                 wcscmp(clsName, L"TopLevelWindowForOverflowXamlIsland") == 0);

        if (isCoreOrXamlHost && (isFullscreenMon || isTallColumn)) {
            std::vector<RECT> subRects;
            if (ensureUia()) {
                ExtractUiaSubWindowRects(pUia, pWalker, hwnd, rc, this, isFullscreenMon, subRects);
            }

            if (!subRects.empty()) {
                RECT unionRc = subRects[0];
                for (const auto& sr : subRects) {
                    PushUniqueWindowRect(desktopWindowRects, sr);
                    unionRc.left   = std::min(unionRc.left, sr.left);
                    unionRc.top    = std::min(unionRc.top, sr.top);
                    unionRc.right  = std::max(unionRc.right, sr.right);
                    unionRc.bottom = std::max(unionRc.bottom, sr.bottom);
                }
                if (subRects.size() > 1) {
                    PushUniqueWindowRect(desktopWindowRects, unionRc);
                }
            }

            // If this is a fullscreen transparent shell canvas (like "Windows Input Experience" TextInputHost.exe),
            // do NOT push the fullscreen container rect itself—only its active child popup rects above.
            bool isInputExperienceCanvas =
                isFullscreenMon &&
                (wcsstr(winTitle, L"Input Experience") != nullptr ||
                 wcsstr(winTitle, L"Text Input") != nullptr ||
                 winTitle[0] == L'\0' ||
                 wcscmp(clsName, L"XamlExplorerHostIslandWindow") == 0);

            if (!isInputExperienceCanvas) {
                PushUniqueWindowRect(desktopWindowRects, rc);
            }
            continue;
        }

        PushUniqueWindowRect(desktopWindowRects, rc);
    }

    if (pWalker) pWalker->Release();
    if (pUia) pUia->Release();

    // Append each monitor's desktop work area (desktop screen without taskbar) at the back of Z-order
    EnumDesktopWindowsCtx ctx{ this, &desktopWindowRects };
    EnumDisplayMonitors(nullptr, nullptr, EnumMonitorWorkAreaProc, (LPARAM)&ctx);
}

void PepperSnapDaemon::SuppressAboveOverlayWindows() {
    typedef BOOL (WINAPI *PFN_GetWindowBand)(HWND, PDWORD);
    static PFN_GetWindowBand s_fnGetWindowBand = nullptr;
    static bool s_resolvedBandFn = false;
    if (!s_resolvedBandFn) {
        HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
        if (hUser32) {
            s_fnGetWindowBand = (PFN_GetWindowBand)GetProcAddress(hUser32, "GetWindowBand");
        }
        s_resolvedBandFn = true;
    }

    HWND hCur = nullptr;
    for (int iter = 0; iter < 1024; ++iter) {
        hCur = FindWindowExW(nullptr, hCur, nullptr, nullptr);
        if (!hCur) break;
        if (!IsWindow(hCur) || !IsWindowVisible(hCur) || IsIconic(hCur)) continue;
        if (hCur == hOverlayWnd || hCur == hOptionsWnd || hCur == hShortcutsWnd || hCur == hTrayWnd) continue;
        if (std::find(pinnedWindows.begin(), pinnedWindows.end(), hCur) != pinnedWindows.end()) continue;

        DWORD cloaked = 0;
        if (SUCCEEDED(DwmGetWindowAttribute(hCur, 14 /* DWMWA_CLOAKED */, &cloaked, sizeof(cloaked))) && cloaked != 0) {
            continue;
        }

        WCHAR clsName[128] = {0};
        GetClassNameW(hCur, clsName, 127);
        if (wcscmp(clsName, L"Progman") == 0 ||
            wcscmp(clsName, L"WorkerW") == 0 ||
            wcscmp(clsName, L"Shell_TrayWnd") == 0 ||
            wcscmp(clsName, L"Shell_SecondaryTrayWnd") == 0 ||
            wcscmp(clsName, L"PepperSnapOverlayWnd") == 0 ||
            wcscmp(clsName, L"PepperSnapOptionsModal") == 0 ||
            wcscmp(clsName, L"PepperSnapShortcutsModal") == 0 ||
            wcscmp(clsName, L"PepperSnapPinWnd") == 0 ||
            wcscmp(clsName, L"PepperSnapTrayDaemonClass") == 0) {
            continue;
        }

        DWORD band = 1; // ZBID_DESKTOP = 1
        if (s_fnGetWindowBand) {
            s_fnGetWindowBand(hCur, &band);
        }

        bool isHigherBand = (band > 1);
        bool isShellNotificationOrFlyout =
            wcscmp(clsName, L"Windows.UI.Core.CoreWindow") == 0 ||
            wcscmp(clsName, L"XamlExplorerHostIslandWindow") == 0 ||
            wcscmp(clsName, L"ControlCenterWindow") == 0 ||
            wcscmp(clsName, L"TopLevelWindowForOverflowXamlIsland") == 0 ||
            wcscmp(clsName, L"Xaml_WindowedPopupClass") == 0 ||
            wcscmp(clsName, L"PopupHost") == 0 ||
            wcscmp(clsName, L"NotifyIconOverflowWindow") == 0 ||
            wcscmp(clsName, L"tooltips_class32") == 0;

        if (!isHigherBand && !isShellNotificationOrFlyout) continue;

        if (std::find(suppressedAboveOverlayWindows.begin(), suppressedAboveOverlayWindows.end(), hCur) == suppressedAboveOverlayWindows.end()) {
            suppressedAboveOverlayWindows.push_back(hCur);
        }

        // Clip the live higher-band window to an empty 0x0 region and hide it so hOverlayWnd (which already captured
        // its frozen pixels and selectable rects) renders on the very top without obstruction
        HRGN hEmptyRgn = CreateRectRgn(0, 0, 0, 0);
        if (hEmptyRgn) {
            if (!SetWindowRgn(hCur, hEmptyRgn, TRUE)) {
                DeleteObject(hEmptyRgn);
            }
        }
        SetWindowPos(hCur, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_HIDEWINDOW);
        ShowWindow(hCur, SW_HIDE);
        PostMessageW(hCur, WM_ACTIVATE, WA_INACTIVE, 0);
        PostMessageW(hCur, WM_CANCELMODE, 0, 0);
    }
}

void PepperSnapDaemon::RestoreSuppressedAboveOverlayWindows() {
    for (HWND hwnd : suppressedAboveOverlayWindows) {
        if (hwnd && IsWindow(hwnd)) {
            SetWindowRgn(hwnd, nullptr, TRUE);
            ShowWindowAsync(hwnd, SW_SHOWNOACTIVATE);
        }
    }
    suppressedAboveOverlayWindows.clear();
}

bool PepperSnapDaemon::UpdateCtrlWindowHover() {
    bool prevHas = hasCtrlHoverWindow;
    RECT prevRc = ctrlHoverWindowRect;

    bool ctrlHeld = ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) ||
                    ((GetKeyState(VK_CONTROL) & 0x8000) != 0) ||
                    ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0) ||
                    ((GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0);
    if (!ctrlHeld || hasSelection || (dragMode != DragMode::None && dragMode != DragMode::PendingOutsideSelection)) {
        hasCtrlHoverWindow = false;
        return (prevHas != hasCtrlHoverWindow);
    }

    bool found = false;
    RECT matched = {0, 0, 0, 0};
    for (const auto& rc : desktopWindowRects) {
        if (PtInRect(&rc, mousePt)) {
            found = true;
            matched = rc;
            break;
        }
    }
    hasCtrlHoverWindow = found;
    if (found) ctrlHoverWindowRect = matched;

    return (prevHas != hasCtrlHoverWindow) ||
           (found && !EqualRect(&prevRc, &ctrlHoverWindowRect));
}

void PepperSnapDaemon::DiscardSavedEditSession() {
    if (savedEditSession.frozenDesktopBmp) {
        delete savedEditSession.frozenDesktopBmp;
        savedEditSession.frozenDesktopBmp = nullptr;
    }
    if (savedEditSession.overlaySingleImageBmp) {
        delete savedEditSession.overlaySingleImageBmp;
        savedEditSession.overlaySingleImageBmp = nullptr;
    }
    for (Bitmap* f : savedEditSession.overlayFrames) {
        delete f;
    }
    savedEditSession.overlayFrames.clear();
    savedEditSession.overlayFrameDelaysMs.clear();
    savedEditSession.annotations.clear();
    savedEditSession.undoStack.clear();
    savedEditSession.redoStack.clear();
    savedEditSession.selectedAnnotationIds.clear();
    savedEditSession = SavedEditOverlaySession();
}

void PepperSnapDaemon::StartRegionSnipOverlay(Bitmap* customBmp, const RECT* customSelRect) {
    if (hOverlayWnd && IsWindow(hOverlayWnd)) {
        if (customBmp || (hOptionsWnd && IsWindow(hOptionsWnd)) || (hShortcutsWnd && IsWindow(hShortcutsWnd))) {
            DiscardSavedEditSession();
            CloseRegionSnipOverlay();
        } else if (!IsEditingOpenedImage()) {
            SetForegroundWindow(hOverlayWnd);
            return;
        } else {
            // Capture the "Edit with PepperSnap" window into Custom Area mode
            KillTimer(hOverlayWnd, 4);
            showingCustomAreaHoverTip = false;
            lastCustomAreaHoverMouse = { -10000, -10000 };
            CommitActiveTextBox();
            CommitActiveSizeInput();
            CommitActiveRatioInput();
            CommitActiveStrokeInput();
            hoveredBtnId = -1;
            pressedBtnId = -1;
            InvalidateRect(hOverlayWnd, nullptr, FALSE);
            UpdateWindow(hOverlayWnd);
            GdiFlush();

            Bitmap* capturedEditBmp = CaptureVirtualDesktop();
            if (!capturedEditBmp) return;

            std::vector<RECT> editWindowRects;
            if (pinnedImageAboveEdit) {
                HWND hPinCur = nullptr;
                while ((hPinCur = FindWindowExW(nullptr, hPinCur, L"PepperSnapPinWnd", nullptr)) != nullptr) {
                    if (IsWindow(hPinCur) && IsWindowVisible(hPinCur)) {
                        PinnedWindowData* pData = (PinnedWindowData*)GetWindowLongPtrW(hPinCur, GWLP_USERDATA);
                        if (pData && pData->bmp) {
                            int pinW = 32, pinH = 32;
                            GetPinnedScaledDims(pData, pinW, pinH);
                            RECT pinRc = {
                                pData->screenImgX - vScreenX,
                                pData->screenImgY - vScreenY,
                                pData->screenImgX - vScreenX + pinW,
                                pData->screenImgY - vScreenY + pinH
                            };
                            editWindowRects.push_back(pinRc);
                        }
                    }
                }
            }
            if (HasOverlayMultiFrames()) {
                if (overlayFrameStripToggleBtnRect.right > overlayFrameStripToggleBtnRect.left &&
                    overlayFrameStripToggleBtnRect.bottom > overlayFrameStripToggleBtnRect.top) {
                    editWindowRects.push_back(overlayFrameStripToggleBtnRect);
                }
                if (!overlayFrameStripCollapsed &&
                    overlayFrameStripPanelRect.right > overlayFrameStripPanelRect.left &&
                    overlayFrameStripPanelRect.bottom > overlayFrameStripPanelRect.top) {
                    editWindowRects.push_back(overlayFrameStripPanelRect);
                }
            }
            if (hasSelection) {
                if (hudBoundsRect.right > hudBoundsRect.left && hudBoundsRect.bottom > hudBoundsRect.top) {
                    editWindowRects.push_back(hudBoundsRect);
                }
                RECT normSel = {
                    std::min(selRect.left, selRect.right),
                    std::min(selRect.top, selRect.bottom),
                    std::max(selRect.left, selRect.right),
                    std::max(selRect.top, selRect.bottom)
                };
                if (normSel.right > normSel.left && normSel.bottom > normSel.top) {
                    editWindowRects.push_back(normSel);
                }
            }
            if (overlayImgDrawRect.right > overlayImgDrawRect.left &&
                overlayImgDrawRect.bottom > overlayImgDrawRect.top) {
                editWindowRects.push_back(overlayImgDrawRect);
            }
            editWindowRects.push_back({ 0, 0, vScreenW, vScreenH });

            DiscardSavedEditSession();
            if (overlayFramesPlaying) {
                KillTimer(hOverlayWnd, 3);
            }
            savedEditSession.active = true;
            savedEditSession.frozenDesktopBmp = frozenDesktopBmp;
            savedEditSession.overlaySingleImageBmp = overlaySingleImageBmp;
            savedEditSession.overlayFrames = std::move(overlayFrames);
            savedEditSession.overlayFrameDelaysMs = std::move(overlayFrameDelaysMs);
            savedEditSession.overlayActiveFrameIdx = overlayActiveFrameIdx;
            savedEditSession.overlayImgDrawRect = overlayImgDrawRect;
            savedEditSession.overlayFrameStripCollapsed = overlayFrameStripCollapsed;
            savedEditSession.overlayFrameStripHeight = overlayFrameStripHeight;
            savedEditSession.overlayFrameScrollX = overlayFrameScrollX;
            savedEditSession.overlayFrameMaxScrollX = overlayFrameMaxScrollX;
            savedEditSession.overlayFramesPlaying = overlayFramesPlaying;
            savedEditSession.hasSelection = hasSelection;
            savedEditSession.selRect = selRect;
            savedEditSession.hasCustomQuad = hasCustomQuad;
            savedEditSession.hasEllipseSelection = hasEllipseSelection;
            for (int i = 0; i < 4; ++i) savedEditSession.selQuad[i] = selQuad[i];
            savedEditSession.annotations = std::move(annotations);
            savedEditSession.undoStack = std::move(undoStack);
            savedEditSession.redoStack = std::move(redoStack);
            savedEditSession.nextStepNum = nextStepNum;
            savedEditSession.nextAnnotationId = nextAnnotationId;
            savedEditSession.selectedAnnotationId = selectedAnnotationId;
            savedEditSession.selectedAnnotationIds = std::move(selectedAnnotationIds);
            savedEditSession.activeTool = activeTool;
            savedEditSession.hasCustomHudPos = hasCustomHudPos;
            savedEditSession.customHudPos = customHudPos;
            savedEditSession.hasCustomRatio = hasCustomRatio;
            savedEditSession.customRatioW = customRatioW;
            savedEditSession.customRatioH = customRatioH;

            frozenDesktopBmp = nullptr;
            overlaySingleImageBmp = nullptr;
            overlayFrames.clear();
            overlayFrameDelaysMs.clear();
            ClearOverlayFrames();

            frozenDesktopBmp = capturedEditBmp;
            desktopWindowRects = std::move(editWindowRects);
            BuildOverlaySurfaceCache();

            hasSelection = false;
            selRect = { 0, 0, 0, 0 };
            hasCustomQuad = false;
            hasEllipseSelection = false;
            altHeldOnSelectionStart = false;
            isDraggingSingleCorner = false;
            hasCtrlHoverWindow = false;
            ctrlHoverWindowRect = { 0, 0, 0, 0 };
            dimPillRect = { 0, 0, 0, 0 };
            ratioPillRect = { 0, 0, 0, 0 };
            ratioResetBtnRect = { 0, 0, 0, 0 };
            customStrokeRect = { 0, 0, 0, 0 };
            dragMode = DragMode::None;
            activeTool = OverlayTool::SelectMove;
            lastCtrlTempSelect = false;
            lastShiftColorPick = false;
            pendingShiftColorPick = false;
            pendingCtrlWindowSelect = false;
            pendingCtrlWindowRect = { 0, 0, 0, 0 };
            loupeMagnification = 5;
            isRightMouseHeld = false;
            activeSelectionRatioW = 0;
            activeSelectionRatioH = 0;
            hasCustomRatio = false;
            customRatioW = 0;
            customRatioH = 0;
            ratioResetBtnPressed = false;
            lastRatioHover = false;
            lastRatioResetHover = false;
            hasCustomHudPos = false;
            annotations.clear();
            undoStack.clear();
            redoStack.clear();
            nextStepNum = 1;
            ClearAnnotationSelection();
            hoveredBtnId = -1;
            pressedBtnId = -1;

            POINT curPt{ 0, 0 };
            if (GetCursorPos(&curPt)) {
                mousePt = { curPt.x - vScreenX, curPt.y - vScreenY };
            }
            UpdateCtrlWindowHover();
            HidePinnedBubbleTooltip();
            SetWindowPos(hOverlayWnd, HWND_TOPMOST, vScreenX, vScreenY, vScreenW, vScreenH, SWP_SHOWWINDOW);
            SetForegroundWindow(hOverlayWnd);
            SetActiveWindow(hOverlayWnd);
            SetFocus(hOverlayWnd);
            UpdateOverlayCursor(mousePt.x, mousePt.y);
            InvalidateRect(hOverlayWnd, nullptr, FALSE);
            UpdateWindow(hOverlayWnd);
            return;
        }
    }
    if (frozenDesktopBmp) {
        delete frozenDesktopBmp;
        frozenDesktopBmp = nullptr;
    }

    if (customBmp) {
        vScreenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
        vScreenY = GetSystemMetrics(SM_YVIRTUALSCREEN);
        vScreenW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        vScreenH = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        desktopWindowRects.clear();
        hasCtrlHoverWindow = false;
        frozenDesktopBmp = customBmp;
        hasSelection = true;
        hasEllipseSelection = false;
        altHeldOnSelectionStart = false;
        if (customSelRect) {
            selRect = *customSelRect;
        } else {
            int cw = std::min(vScreenW, (int)customBmp->GetWidth());
            int ch = std::min(vScreenH, (int)customBmp->GetHeight());
            int cx = (vScreenW - cw) / 2;
            int cy = (vScreenH - ch) / 2;
            selRect = { cx, cy, cx + cw, cy + ch };
        }
        SyncQuadFromSelRect();
    } else {
        ClearOverlayFrames();
        frozenDesktopBmp = CaptureVirtualDesktop();
        hasSelection = false;
        hasCustomQuad = false;
        hasEllipseSelection = false;
        altHeldOnSelectionStart = false;
        SnapshotDesktopWindows();
    }
    if (!frozenDesktopBmp) return;
    BuildOverlaySurfaceCache();

    isSavingAsModalOpen = false;
    isDraggingSingleCorner = false;
    dragMode = DragMode::None;
    hoveredBtnId = -1;
    pressedBtnId = -1;
    hasCustomHudPos = false;
    lastPillHover = false;
    lastStrokeHover = false;
    showingCustomAreaHoverTip = false;
    lastCustomAreaHoverMouse = { -10000, -10000 };
    activeTool = OverlayTool::SelectMove;
    lastCtrlTempSelect = false;
    lastShiftColorPick = false;
    pendingShiftColorPick = false;
    pendingCtrlWindowSelect = false;
    pendingCtrlWindowRect = { 0, 0, 0, 0 };
    loupeMagnification = 5;
    isRightMouseHeld = false;
    activeSelectionRatioW = 0;
    activeSelectionRatioH = 0;
    dimPillRect = { 0, 0, 0, 0 };
    ratioPillRect = { 0, 0, 0, 0 };
    ratioResetBtnRect = { 0, 0, 0, 0 };
    isEditingText = false;
    isEditingSize = false;
    isDraggingSizeText = false;
    editingSizeText.clear();
    sizeCaretPos = sizeSelAnchor = 0;
    isEditingRatio = false;
    isDraggingRatioText = false;
    editingRatioText.clear();
    ratioCaretPos = ratioSelAnchor = 0;
    ratioResetBtnPressed = false;
    lastRatioHover = false;
    lastRatioResetHover = false;
    isEditingStroke = false;
    isDraggingStrokeText = false;
    editingStrokeText.clear();
    strokeCaretPos = strokeSelAnchor = 0;
    selectedAnnotationId = -1;
    annotations.clear();
    undoStack.clear();
    redoStack.clear();
    nextStepNum = 1;
    GetCursorPos(&mousePt);
    mousePt.x -= vScreenX;
    mousePt.y -= vScreenY;
    selectionTargetPt = mousePt;
    if (!hasSelection) {
        UpdateCtrlWindowHover();
    }

    typedef HWND (WINAPI *PFN_CreateWindowInBand)(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID, DWORD);
    typedef BOOL (WINAPI *PFN_SetWindowBand)(HWND, HWND, DWORD);
    static PFN_CreateWindowInBand s_fnCreateWindowInBand = nullptr;
    static PFN_SetWindowBand s_fnSetWindowBand = nullptr;
    static bool s_resolvedWinBandApis = false;
    if (!s_resolvedWinBandApis) {
        HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
        if (hUser32) {
            s_fnCreateWindowInBand = (PFN_CreateWindowInBand)GetProcAddress(hUser32, "CreateWindowInBand");
            s_fnSetWindowBand      = (PFN_SetWindowBand)GetProcAddress(hUser32, "SetWindowBand");
        }
        s_resolvedWinBandApis = true;
    }

    hOverlayWnd = nullptr;
    if (!customBmp && s_fnCreateWindowInBand) {
        const DWORD candidateBands[] = { 16 /* ZBID_SYSTEM_TOOLS */, 4 /* ZBID_IMMERSIVE_NOTIFICATIONS */, 2 /* ZBID_UIACCESS */ };
        for (DWORD b : candidateBands) {
            hOverlayWnd = s_fnCreateWindowInBand(
                WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                L"PepperSnapOverlayWnd", L"PepperSnap Region Snip",
                WS_POPUP | WS_VISIBLE,
                vScreenX, vScreenY, vScreenW, vScreenH,
                nullptr, nullptr, hInst, nullptr, b
            );
            if (hOverlayWnd) break;
        }
    }
    if (!hOverlayWnd) {
        hOverlayWnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            L"PepperSnapOverlayWnd", L"PepperSnap Region Snip",
            WS_POPUP | WS_VISIBLE,
            vScreenX, vScreenY, vScreenW, vScreenH,
            nullptr, nullptr, hInst, nullptr
        );
    }
    if (!customBmp && hOverlayWnd && s_fnSetWindowBand) {
        if (!s_fnSetWindowBand(hOverlayWnd, HWND_TOPMOST, 16 /* ZBID_SYSTEM_TOOLS */)) {
            if (!s_fnSetWindowBand(hOverlayWnd, HWND_TOPMOST, 4 /* ZBID_IMMERSIVE_NOTIFICATIONS */)) {
                s_fnSetWindowBand(hOverlayWnd, HWND_TOPMOST, 2 /* ZBID_UIACCESS */);
            }
        }
    }

    // Immediately suppress/clip any live higher-band notification sidebars or notification pop-ups
    // (their pixels and selectable window rects are already captured in frozenDesktopBmp & desktopWindowRects)
    SuppressAboveOverlayWindows();

    SetWindowPos(hOverlayWnd, HWND_TOPMOST, vScreenX, vScreenY, vScreenW, vScreenH, SWP_SHOWWINDOW);
    if (IsEditingOpenedImage() && pinnedImageAboveEdit) {
        RaisePinnedWindowsAboveOverlay();
    } else if (IsEditingOpenedImage()) {
        HidePinnedBubbleTooltip();
    }
    UpdateOverlayCursor(mousePt.x, mousePt.y);

    // Unlock foreground activation so SetForegroundWindow succeeds even when a UWP CoreWindow (Notification Center / Start) had focus
    LockSetForegroundWindow(LSFW_UNLOCK);
    keybd_event(0, 0, KEYEVENTF_KEYUP, 0);

    HWND hFore = GetForegroundWindow();
    DWORD foreThread = hFore ? GetWindowThreadProcessId(hFore, nullptr) : 0;
    DWORD curThread = GetCurrentThreadId();
    if (foreThread && foreThread != curThread) {
        AttachThreadInput(foreThread, curThread, TRUE);
        BringWindowToTop(hOverlayWnd);
        SetForegroundWindow(hOverlayWnd);
        SetActiveWindow(hOverlayWnd);
        SetFocus(hOverlayWnd);
        AttachThreadInput(foreThread, curThread, FALSE);
    } else {
        BringWindowToTop(hOverlayWnd);
        SetForegroundWindow(hOverlayWnd);
        SetActiveWindow(hOverlayWnd);
        SetFocus(hOverlayWnd);
    }
    if (IsEditingOpenedImage() && pinnedImageAboveEdit) {
        RaisePinnedWindowsAboveOverlay();
    }
}

bool PepperSnapDaemon::IsPinnedWindow(HWND hWnd) const {
    if (!hWnd || !IsWindow(hWnd)) return false;
    if (std::find(pinnedWindows.begin(), pinnedWindows.end(), hWnd) != pinnedWindows.end()) {
        return true;
    }
    WCHAR clsName[64] = {0};
    if (GetClassNameW(hWnd, clsName, 63) > 0 && wcscmp(clsName, L"PepperSnapPinWnd") == 0) {
        return true;
    }
    return false;
}

HWND PepperSnapDaemon::GetLowestPinnedWindow() const {
    for (HWND hp : pinnedWindows) {
        if (hp && IsWindow(hp) && IsWindowVisible(hp)) {
            return hp;
        }
    }
    HWND hPinExtra = FindWindowExW(nullptr, nullptr, L"PepperSnapPinWnd", nullptr);
    if (hPinExtra && IsWindow(hPinExtra) && IsWindowVisible(hPinExtra)) {
        return hPinExtra;
    }
    return nullptr;
}

void PepperSnapDaemon::RaisePinnedWindowsAboveOverlay() {
    if (!pinnedImageAboveEdit) return;
    for (HWND hp : pinnedWindows) {
        if (hp && IsWindow(hp) && IsWindowVisible(hp)) {
            SetWindowPos(hp, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }
    }
    HWND hPinExtra = nullptr;
    while ((hPinExtra = FindWindowExW(nullptr, hPinExtra, L"PepperSnapPinWnd", nullptr)) != nullptr) {
        if (std::find(pinnedWindows.begin(), pinnedWindows.end(), hPinExtra) == pinnedWindows.end()) {
            if (IsWindowVisible(hPinExtra)) {
                SetWindowPos(hPinExtra, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
            }
        }
    }
    if (g_hPinTooltipWnd && IsWindow(g_hPinTooltipWnd) && IsWindowVisible(g_hPinTooltipWnd)) {
        SetWindowPos(g_hPinTooltipWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

// ----------------------------------------------------------------------------
// Getting the overlay off the screen before slow work
//
// A click on Copy, Pin, Quick save, Extract text or Close used to keep the
// toolbar on screen through everything the button does. Two of those things are
// slow: RenderCroppedRegionBitmap() alone is about 100 ms on a full-screen
// custom-area selection (the crop clone plus the mosaic and highlighter passes),
// and the encode, the clipboard handoff and OCR add a few hundred more. So the
// overlay sat there frozen and only then did the job, which reads - correctly -
// as the click having been ignored.
//
// The bitmap those actions work from is cut out of daemon state, not out of the
// window, so the window can leave first. It cannot simply be closed first,
// because closing deletes that state, so it is hidden now and closed when the
// work is done. Hiding is immediate; the work then happens with nothing on
// screen, which is what "throw it to the background" means here.
//
// One case must not hide: when the overlay is sitting on top of an image opened
// for editing, closing hands the window back to that edit session (the
// savedEditSession path in CloseRegionSnipOverlay) rather than taking it away,
// and a window that is never shown again is a stuck app.
// ----------------------------------------------------------------------------
bool PepperSnapDaemon::HideOverlayForPendingWork() {
    if (savedEditSession.active) return false;
    if (!hOverlayWnd || !IsWindow(hOverlayWnd) || !IsWindowVisible(hOverlayWnd)) return false;
    // Nothing shows this window again except the two entry points that create it,
    // so a plain hide holds. The 40 ms poll is stopped only because its hover and
    // window-enumeration work has nothing to show for itself while hidden.
    KillTimer(hOverlayWnd, 2);
    KillTimer(hOverlayWnd, 4);
    ShowWindow(hOverlayWnd, SW_HIDE);
    return true;
}

void PepperSnapDaemon::RestoreOverlayAfterPendingWork(bool wasHidden) {
    if (!wasHidden || !hOverlayWnd || !IsWindow(hOverlayWnd)) return;
    ShowWindow(hOverlayWnd, SW_SHOW);
    SetForegroundWindow(hOverlayWnd);
    SetFocus(hOverlayWnd);
    SetTimer(hOverlayWnd, 2, 40, nullptr);
    InvalidateRect(hOverlayWnd, nullptr, FALSE);
}

void PepperSnapDaemon::CloseRegionSnipOverlay() {
    isSavingAsModalOpen = false;
    hasCustomHudPos = false;
    showingCustomAreaHoverTip = false;
    lastCustomAreaHoverMouse = { -10000, -10000 };
    if (hOverlayWnd && IsWindow(hOverlayWnd)) {
        KillTimer(hOverlayWnd, 4);
    }
    activeTool = OverlayTool::SelectMove;
    lastCtrlTempSelect = false;
    lastShiftColorPick = false;
    pendingShiftColorPick = false;
    pendingCtrlWindowSelect = false;
    pendingCtrlWindowRect = { 0, 0, 0, 0 };
    loupeMagnification = 5;
    isRightMouseHeld = false;
    activeSelectionRatioW = 0;
    activeSelectionRatioH = 0;
    dimPillRect = { 0, 0, 0, 0 };
    ratioPillRect = { 0, 0, 0, 0 };
    ratioResetBtnRect = { 0, 0, 0, 0 };
    isEditingText = false;
    isEditingSize = false;
    isDraggingSizeText = false;
    editingSizeText.clear();
    sizeCaretPos = sizeSelAnchor = 0;
    isEditingRatio = false;
    isDraggingRatioText = false;
    editingRatioText.clear();
    ratioCaretPos = ratioSelAnchor = 0;
    ratioResetBtnPressed = false;
    lastRatioHover = false;
    lastRatioResetHover = false;
    isEditingStroke = false;
    isDraggingStrokeText = false;
    editingStrokeText.clear();
    strokeCaretPos = strokeSelAnchor = 0;
    if (savedEditSession.active && hOverlayWnd && IsWindow(hOverlayWnd)) {
        if (frozenDesktopBmp) {
            delete frozenDesktopBmp;
            frozenDesktopBmp = nullptr;
        }
        ClearOverlayFrames();

        frozenDesktopBmp = savedEditSession.frozenDesktopBmp;
        overlaySingleImageBmp = savedEditSession.overlaySingleImageBmp;
        overlayFrames = std::move(savedEditSession.overlayFrames);
        overlayFrameDelaysMs = std::move(savedEditSession.overlayFrameDelaysMs);
        overlayActiveFrameIdx = savedEditSession.overlayActiveFrameIdx;
        overlayImgDrawRect = savedEditSession.overlayImgDrawRect;
        overlayFrameStripCollapsed = savedEditSession.overlayFrameStripCollapsed;
        overlayFrameStripHeight = savedEditSession.overlayFrameStripHeight;
        overlayFrameScrollX = savedEditSession.overlayFrameScrollX;
        overlayFrameMaxScrollX = savedEditSession.overlayFrameMaxScrollX;
        overlayFramesPlaying = savedEditSession.overlayFramesPlaying;
        hasSelection = savedEditSession.hasSelection;
        selRect = savedEditSession.selRect;
        hasCustomQuad = savedEditSession.hasCustomQuad;
        hasEllipseSelection = savedEditSession.hasEllipseSelection;
        for (int i = 0; i < 4; ++i) selQuad[i] = savedEditSession.selQuad[i];
        annotations = std::move(savedEditSession.annotations);
        undoStack = std::move(savedEditSession.undoStack);
        redoStack = std::move(savedEditSession.redoStack);
        nextStepNum = savedEditSession.nextStepNum;
        nextAnnotationId = savedEditSession.nextAnnotationId;
        selectedAnnotationId = savedEditSession.selectedAnnotationId;
        selectedAnnotationIds = std::move(savedEditSession.selectedAnnotationIds);
        activeTool = savedEditSession.activeTool;
        hasCustomHudPos = savedEditSession.hasCustomHudPos;
        customHudPos = savedEditSession.customHudPos;
        hasCustomRatio = savedEditSession.hasCustomRatio;
        customRatioW = savedEditSession.customRatioW;
        customRatioH = savedEditSession.customRatioH;
        savedEditSession = SavedEditOverlaySession();

        hasCtrlHoverWindow = false;
        desktopWindowRects.clear();
        dragMode = DragMode::None;
        hoveredBtnId = -1;
        pressedBtnId = -1;

        BuildOverlaySurfaceCache();
        LayoutOverlayFrameStrip(vScreenW, vScreenH);
        BuildDockedHUD();
        if (overlayFramesPlaying && HasOverlayMultiFrames()) {
            int delayMs = (overlayActiveFrameIdx < overlayFrameDelaysMs.size()) ? overlayFrameDelaysMs[overlayActiveFrameIdx] : 80;
            delayMs = std::max(20, std::min(60000, delayMs));
            SetTimer(hOverlayWnd, 3, (UINT)delayMs, nullptr);
        }
        if (pinnedImageAboveEdit) {
            RaisePinnedWindowsAboveOverlay();
        }
        POINT curPt{ 0, 0 };
        if (GetCursorPos(&curPt)) {
            mousePt = { curPt.x - vScreenX, curPt.y - vScreenY };
        }
        SetForegroundWindow(hOverlayWnd);
        SetActiveWindow(hOverlayWnd);
        SetFocus(hOverlayWnd);
        UpdateOverlayCursor(mousePt.x, mousePt.y);
        InvalidateRect(hOverlayWnd, nullptr, FALSE);
        UpdateWindow(hOverlayWnd);
        return;
    }

    if (hOverlayWnd) {
        DestroyWindow(hOverlayWnd);
        hOverlayWnd = nullptr;
    }
    // The window is off the screen now. Everything left is bookkeeping the user
    // cannot see - and there is a lot of it: the two SaveSettings() calls below
    // each rewrite the whole ini file about 130 times, and the pair measured
    // 300-420 ms. Before this ordering existed that time was spent with the
    // toolbar still drawn, which is the freeze a close used to be.
    RecordLastCustomSelection();
    SaveSettings();
    RestoreSuppressedAboveOverlayWindows();
    FreeOverlaySurfaceCache();
    ClearOverlayFrames();
    if (frozenDesktopBmp) {
        delete frozenDesktopBmp;
        frozenDesktopBmp = nullptr;
    }
    hasSelection = false;
    hasCtrlHoverWindow = false;
    desktopWindowRects.clear();
    dragMode = DragMode::None;
    hoveredBtnId = -1;
    pressedBtnId = -1;
}

struct WebPAnimFrameInfo {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    int durationMs = 80;
    bool disposeBg = false;
    bool noBlend = false;
};

static bool ParseWebPAnimationHeader(const std::wstring& filePath, int& outCanvasW, int& outCanvasH, std::vector<WebPAnimFrameInfo>& outAnmfList) {
    outCanvasW = 0;
    outCanvasH = 0;
    outAnmfList.clear();

    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    DWORD fileSize = GetFileSize(hFile, nullptr);
    if (fileSize < 30 || fileSize == INVALID_FILE_SIZE || fileSize > 256 * 1024 * 1024) {
        CloseHandle(hFile);
        return false;
    }
    std::vector<BYTE> buf(fileSize);
    DWORD bytesRead = 0;
    bool ok = ReadFile(hFile, buf.data(), fileSize, &bytesRead, nullptr) && (bytesRead == fileSize);
    CloseHandle(hFile);
    if (!ok) return false;

    if (memcmp(buf.data(), "RIFF", 4) != 0 || memcmp(buf.data() + 8, "WEBP", 4) != 0) return false;

    auto readU24LE = [](const BYTE* p) -> int {
        return (int)p[0] | ((int)p[1] << 8) | ((int)p[2] << 16);
    };

    size_t pos = 12;
    while (pos + 8 <= buf.size()) {
        uint32_t chunkSize = 0;
        memcpy(&chunkSize, buf.data() + pos + 4, 4);
        size_t payloadPos = pos + 8;
        if (payloadPos + chunkSize > buf.size()) break;

        if (memcmp(buf.data() + pos, "VP8X", 4) == 0 && chunkSize >= 10) {
            const BYTE* p = buf.data() + payloadPos;
            outCanvasW = readU24LE(p + 4) + 1;
            outCanvasH = readU24LE(p + 7) + 1;
        } else if (memcmp(buf.data() + pos, "ANMF", 4) == 0 && chunkSize >= 16) {
            const BYTE* p = buf.data() + payloadPos;
            WebPAnimFrameInfo info;
            info.x = readU24LE(p + 0) * 2;
            info.y = readU24LE(p + 3) * 2;
            info.w = readU24LE(p + 6) + 1;
            info.h = readU24LE(p + 9) + 1;
            info.durationMs = readU24LE(p + 12);
            if (info.durationMs <= 10) info.durationMs = 80;
            info.durationMs = std::max(20, std::min(60000, info.durationMs));
            BYTE flags = p[15];
            info.disposeBg = (flags & 0x01) != 0;
            info.noBlend   = (flags & 0x02) != 0;
            outAnmfList.push_back(info);
        }
        pos += 8 + (size_t)chunkSize + ((size_t)chunkSize & 1u);
    }
    return (outCanvasW > 0 && outCanvasH > 0);
}

static bool LoadImageFramesWithWIC(const std::wstring& filePath, std::vector<Bitmap*>& outFrames, std::vector<int>& outDelaysMs) {
    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool needCoUninit = SUCCEEDED(hrCo);

    int webpCanvasW = 0, webpCanvasH = 0;
    std::vector<WebPAnimFrameInfo> webpAnmf;
    ParseWebPAnimationHeader(filePath, webpCanvasW, webpCanvasH, webpAnmf);

    const CLSID clsidWicFactory2  = { 0x317d06e8, 0x5f24, 0x433d, { 0xbd, 0xf7, 0x79, 0xce, 0x68, 0xd8, 0xab, 0xc2 } };
    const CLSID clsidWicFactory1  = { 0xcacaf262, 0x9370, 0x4615, { 0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a } };
    const IID   iidWicFactory     = { 0xec5ec8a9, 0xc395, 0x4314, { 0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70 } };
    const CLSID clsidWebpDecoder  = { 0x7693e886, 0x51c9, 0x4070, { 0x84, 0x19, 0x9f, 0x70, 0x73, 0x8e, 0xc8, 0xfa } };
    const IID   iidWicDecoder     = { 0x9edde9e7, 0x8dee, 0x47ea, { 0x99, 0xdf, 0xe6, 0xfa, 0xf2, 0xed, 0x44, 0xbf } };
    const GUID  fmt32bppBGRA      = { 0x6fddc324, 0x4e03, 0x4bfe, { 0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0f } };

    IWICImagingFactory* pFactory = nullptr;
    if (FAILED(CoCreateInstance(clsidWicFactory2, nullptr, CLSCTX_INPROC_SERVER, iidWicFactory, (void**)&pFactory)) || !pFactory) {
        CoCreateInstance(clsidWicFactory1, nullptr, CLSCTX_INPROC_SERVER, iidWicFactory, (void**)&pFactory);
    }

    if (pFactory) {
        IWICBitmapDecoder* pDecoder = nullptr;
        IWICStream* pStream = nullptr;
        if (FAILED(pFactory->CreateDecoderFromFilename(filePath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &pDecoder)) || !pDecoder) {
            if (SUCCEEDED(pFactory->CreateStream(&pStream)) && pStream) {
                if (SUCCEEDED(pStream->InitializeFromFilename(filePath.c_str(), GENERIC_READ))) {
                    if (FAILED(pFactory->CreateDecoderFromStream(pStream, nullptr, WICDecodeMetadataCacheOnDemand, &pDecoder)) || !pDecoder) {
                        if (SUCCEEDED(CoCreateInstance(clsidWebpDecoder, nullptr, CLSCTX_INPROC_SERVER, iidWicDecoder, (void**)&pDecoder)) && pDecoder) {
                            LARGE_INTEGER zero = {0};
                            pStream->Seek(zero, STREAM_SEEK_SET, nullptr);
                            if (FAILED(pDecoder->Initialize(pStream, WICDecodeMetadataCacheOnDemand))) {
                                pDecoder->Release();
                                pDecoder = nullptr;
                            }
                        }
                    }
                }
            }
        }

        if (pDecoder) {
            UINT frameCount = 0;
            if (FAILED(pDecoder->GetFrameCount(&frameCount)) || frameCount < 1) frameCount = 1;

            int canvasW = (webpCanvasW > 0) ? webpCanvasW : 0;
            int canvasH = (webpCanvasH > 0) ? webpCanvasH : 0;
            std::vector<DWORD> accumCanvas;
            if (frameCount > 1 && canvasW > 0 && canvasH > 0) {
                accumCanvas.assign((size_t)canvasW * (size_t)canvasH, 0u);
            }

            for (UINT i = 0; i < frameCount; ++i) {
                IWICBitmapFrameDecode* pFrame = nullptr;
                if (SUCCEEDED(pDecoder->GetFrame(i, &pFrame)) && pFrame) {
                    UINT w = 0, h = 0;
                    if (SUCCEEDED(pFrame->GetSize(&w, &h)) && w > 0 && h > 0) {
                        if (canvasW <= 0 || canvasH <= 0) {
                            canvasW = (int)w;
                            canvasH = (int)h;
                            if (frameCount > 1) {
                                accumCanvas.assign((size_t)canvasW * (size_t)canvasH, 0u);
                            }
                        }

                        int delayMs = (i < webpAnmf.size()) ? webpAnmf[i].durationMs : 80;
                        int frameX  = (i < webpAnmf.size()) ? webpAnmf[i].x : 0;
                        int frameY  = (i < webpAnmf.size()) ? webpAnmf[i].y : 0;
                        bool disposeBg = (i < webpAnmf.size()) ? webpAnmf[i].disposeBg : false;
                        bool noBlend   = (i < webpAnmf.size()) ? webpAnmf[i].noBlend   : false;

                        if (frameCount > 1) {
                            IWICMetadataQueryReader* pMeta = nullptr;
                            if (SUCCEEDED(pFrame->GetMetadataQueryReader(&pMeta)) && pMeta) {
                                PROPVARIANT pv;
                                PropVariantInit(&pv);
                                if (SUCCEEDED(pMeta->GetMetadataByName(L"/ANMF/FrameDuration", &pv))) {
                                    if (pv.vt == VT_UI4 || pv.vt == VT_I4) delayMs = (int)pv.ulVal;
                                    else if (pv.vt == VT_UI2 || pv.vt == VT_I2) delayMs = (int)pv.uiVal;
                                } else if (SUCCEEDED(pMeta->GetMetadataByName(L"/grctlext/Delay", &pv))) {
                                    if (pv.vt == VT_UI2 || pv.vt == VT_I2) delayMs = (int)pv.uiVal * 10;
                                }
                                PropVariantClear(&pv);
                                pMeta->Release();
                            }
                            if (delayMs <= 10) delayMs = 80;
                            delayMs = std::max(20, std::min(60000, delayMs));
                        }

                        IWICFormatConverter* pConv = nullptr;
                        if (SUCCEEDED(pFactory->CreateFormatConverter(&pConv)) && pConv) {
                            if (SUCCEEDED(pConv->Initialize(pFrame, fmt32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
                                bool useCanvasComp = (frameCount > 1 && !accumCanvas.empty() && canvasW > 0 && canvasH > 0 &&
                                                      (!webpAnmf.empty() || (int)w != canvasW || (int)h != canvasH));
                                if (useCanvasComp) {
                                    std::vector<DWORD> rawPx((size_t)w * (size_t)h, 0u);
                                    HRESULT hrCopy = pConv->CopyPixels(nullptr, w * 4u, (UINT)(rawPx.size() * 4u), (BYTE*)rawPx.data());
                                    if (SUCCEEDED(hrCopy)) {
                                        // If WIC already returned a full-canvas frame while ANMF had an offset, place at (0,0)
                                        int dstOffX = ((int)w == canvasW && (int)h == canvasH) ? 0 : frameX;
                                        int dstOffY = ((int)w == canvasW && (int)h == canvasH) ? 0 : frameY;
                                        for (UINT sy = 0; sy < h; ++sy) {
                                            int dy = dstOffY + (int)sy;
                                            if (dy < 0 || dy >= canvasH) continue;
                                            const DWORD* srcRow = rawPx.data() + (size_t)sy * w;
                                            DWORD* dstRow = accumCanvas.data() + (size_t)dy * canvasW;
                                            for (UINT sx = 0; sx < w; ++sx) {
                                                int dx = dstOffX + (int)sx;
                                                if (dx < 0 || dx >= canvasW) continue;
                                                DWORD sp = srcRow[sx];
                                                if (noBlend || i == 0) {
                                                    dstRow[dx] = sp;
                                                } else {
                                                    DWORD sa = (sp >> 24) & 0xFFu;
                                                    if (sa == 255u) {
                                                        dstRow[dx] = sp;
                                                    } else if (sa > 0u) {
                                                        DWORD dp = dstRow[dx];
                                                        DWORD da = (dp >> 24) & 0xFFu;
                                                        if (da == 0u) {
                                                            dstRow[dx] = sp;
                                                        } else {
                                                            DWORD invSa = 255u - sa;
                                                            DWORD outA = sa + (da * invSa + 127u) / 255u;
                                                            if (outA > 0u) {
                                                                DWORD sr = (sp >> 16) & 0xFFu, sg = (sp >> 8) & 0xFFu, sb = sp & 0xFFu;
                                                                DWORD dr = (dp >> 16) & 0xFFu, dg = (dp >> 8) & 0xFFu, db = dp & 0xFFu;
                                                                DWORD outR = (sr * sa * 255u + dr * da * invSa + (outA * 255u) / 2u) / (outA * 255u);
                                                                DWORD outG = (sg * sa * 255u + dg * da * invSa + (outA * 255u) / 2u) / (outA * 255u);
                                                                DWORD outB = (sb * sa * 255u + db * da * invSa + (outA * 255u) / 2u) / (outA * 255u);
                                                                dstRow[dx] = (std::min((DWORD)255, outA) << 24) |
                                                                             (std::min((DWORD)255, outR) << 16) |
                                                                             (std::min((DWORD)255, outG) << 8)  |
                                                                             std::min((DWORD)255, outB);
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }

                                        Bitmap* frameBmp = new Bitmap(canvasW, canvasH, PixelFormat32bppARGB);
                                        Rect lockRc(0, 0, canvasW, canvasH);
                                        BitmapData bd = {};
                                        if (frameBmp->LockBits(&lockRc, ImageLockModeWrite, PixelFormat32bppARGB, &bd) == Ok) {
                                            for (int y = 0; y < canvasH; ++y) {
                                                memcpy((BYTE*)bd.Scan0 + (size_t)y * bd.Stride,
                                                       accumCanvas.data() + (size_t)y * canvasW,
                                                       (size_t)canvasW * 4u);
                                            }
                                            frameBmp->UnlockBits(&bd);
                                            outFrames.push_back(frameBmp);
                                            outDelaysMs.push_back(delayMs);
                                        } else {
                                            delete frameBmp;
                                        }

                                        if (disposeBg) {
                                            for (UINT sy = 0; sy < h; ++sy) {
                                                int dy = dstOffY + (int)sy;
                                                if (dy < 0 || dy >= canvasH) continue;
                                                DWORD* dstRow = accumCanvas.data() + (size_t)dy * canvasW;
                                                for (UINT sx = 0; sx < w; ++sx) {
                                                    int dx = dstOffX + (int)sx;
                                                    if (dx >= 0 && dx < canvasW) dstRow[dx] = 0u;
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    Bitmap* frameBmp = new Bitmap((INT)w, (INT)h, PixelFormat32bppARGB);
                                    Rect lockRc(0, 0, (INT)w, (INT)h);
                                    BitmapData bd = {};
                                    if (frameBmp->LockBits(&lockRc, ImageLockModeWrite, PixelFormat32bppARGB, &bd) == Ok) {
                                        HRESULT hrCopy = pConv->CopyPixels(nullptr, (UINT)bd.Stride, (UINT)((size_t)bd.Stride * h), (BYTE*)bd.Scan0);
                                        frameBmp->UnlockBits(&bd);
                                        if (SUCCEEDED(hrCopy)) {
                                            outFrames.push_back(frameBmp);
                                            outDelaysMs.push_back(delayMs);
                                        } else {
                                            delete frameBmp;
                                        }
                                    } else {
                                        delete frameBmp;
                                    }
                                }
                            }
                            pConv->Release();
                        }
                    }
                    pFrame->Release();
                }
            }
            pDecoder->Release();
        }
        if (pStream) pStream->Release();
        pFactory->Release();
    }

    if (needCoUninit) CoUninitialize();
    return !outFrames.empty();
}

bool LoadImageFramesFromFile(const std::wstring& filePath, std::vector<Bitmap*>& outFrames, std::vector<int>& outDelaysMs) {
    outFrames.clear();
    outDelaysMs.clear();
    if (filePath.empty()) return false;

    std::wstring ext = PathFindExtensionW(filePath.c_str());
    for (wchar_t& c : ext) c = (wchar_t)towlower(c);

    // For non-WebP formats (including animated .gif, .png, .jpg, .bmp, .tiff, .ico), use GDI+ first
    //    so multi-frame GIF disposal methods and frame sub-rects composite accurately with full alpha
    if (ext != L".webp") {
        Bitmap loaded(filePath.c_str());
        if (loaded.GetLastStatus() == Ok && loaded.GetWidth() > 0 && loaded.GetHeight() > 0) {
            int imgW = (int)loaded.GetWidth();
            int imgH = (int)loaded.GetHeight();
            GUID timeDim = FrameDimensionTime;
            UINT frameCount = loaded.GetFrameCount(&timeDim);
            if (frameCount < 1) frameCount = 1;

            std::vector<int> delays(frameCount, 100);
            if (frameCount > 1) {
                UINT propSize = loaded.GetPropertyItemSize(PropertyTagFrameDelay);
                if (propSize > sizeof(PropertyItem)) {
                    std::vector<BYTE> propBuf(propSize);
                    PropertyItem* pItem = (PropertyItem*)propBuf.data();
                    if (loaded.GetPropertyItem(PropertyTagFrameDelay, propSize, pItem) == Ok && pItem->value) {
                        const UINT* rawDelays = (const UINT*)pItem->value;
                        size_t count = pItem->length / sizeof(UINT);
                        for (UINT i = 0; i < frameCount; ++i) {
                            UINT cs = (i < count) ? rawDelays[i] : 10;
                            int ms = (int)cs * 10;
                            if (ms <= 10) ms = 100;
                            delays[i] = std::max(20, std::min(60000, ms));
                        }
                    }
                }
            }

            for (UINT i = 0; i < frameCount; ++i) {
                if (frameCount > 1) {
                    loaded.SelectActiveFrame(&timeDim, i);
                }
                Bitmap* frameBmp = new Bitmap(imgW, imgH, PixelFormat32bppARGB);
                Rect lockRc(0, 0, imgW, imgH);
                BitmapData srcBd = {}, dstBd = {};
                if (loaded.LockBits(&lockRc, ImageLockModeRead, PixelFormat32bppARGB, &srcBd) == Ok) {
                    if (frameBmp->LockBits(&lockRc, ImageLockModeWrite, PixelFormat32bppARGB, &dstBd) == Ok) {
                        for (int y = 0; y < imgH; ++y) {
                            memcpy((BYTE*)dstBd.Scan0 + (size_t)y * dstBd.Stride,
                                   (const BYTE*)srcBd.Scan0 + (size_t)y * srcBd.Stride,
                                   (size_t)imgW * 4);
                        }
                        frameBmp->UnlockBits(&dstBd);
                    }
                    loaded.UnlockBits(&srcBd);
                } else {
                    Graphics g(frameBmp);
                    g.Clear(Color(0, 0, 0, 0));
                    g.SetCompositingMode(CompositingModeSourceOver);
                    g.SetInterpolationMode(InterpolationModeNearestNeighbor);
                    g.SetPixelOffsetMode(PixelOffsetModeHalf);
                    g.DrawImage(&loaded, Rect(0, 0, imgW, imgH), 0, 0, imgW, imgH, UnitPixel);
                }
                outFrames.push_back(frameBmp);
                outDelaysMs.push_back(delays[i]);
            }
            return !outFrames.empty();
        }
    }

    // Decode via Windows Imaging Component (WIC — supports .webp lossy/lossless/alpha/animated + all Windows codecs)
    return LoadImageFramesWithWIC(filePath, outFrames, outDelaysMs);
}

void PepperSnapDaemon::ClearOverlayFrames() {
    if (overlaySingleImageBmp) {
        delete overlaySingleImageBmp;
        overlaySingleImageBmp = nullptr;
    }
    for (Bitmap* f : overlayFrames) {
        delete f;
    }
    overlayFrames.clear();
    overlayFrameDelaysMs.clear();
    overlayFrameCardRects.clear();
    overlayActiveFrameIdx = 0;
    overlayImgDrawRect = { 0, 0, 0, 0 };
    overlayFrameStripCollapsed = false;
    overlayFrameScrollX = 0.0f;
    overlayFrameMaxScrollX = 0.0f;
    overlayHoveredFrameIdx = -1;
    overlayToggleBtnHovered = false;
    overlayPlayBtnHovered = false;
    if (overlayFramesPlaying && hOverlayWnd && IsWindow(hOverlayWnd)) {
        KillTimer(hOverlayWnd, 3);
    }
    overlayFramesPlaying = false;
    overlayTopResizeHovered = false;
    overlayScrollbarHovered = false;
    overlayScrollLeftBtnHovered = false;
    overlayScrollRightBtnHovered = false;
    overlayFrameStripPanelRect = { 0, 0, 0, 0 };
    overlayFrameStripToggleBtnRect = { 0, 0, 0, 0 };
    overlayFrameStripPlayBtnRect = { 0, 0, 0, 0 };
    overlayFrameStripResizeRect = { 0, 0, 0, 0 };
    overlayFrameStripScrollTrackRect = { 0, 0, 0, 0 };
    overlayFrameStripScrollThumbRect = { 0, 0, 0, 0 };
    overlayFrameStripScrollLeftRect = { 0, 0, 0, 0 };
    overlayFrameStripScrollRightRect = { 0, 0, 0, 0 };
}

void PepperSnapDaemon::EnsureActiveFrameVisibleInStrip() {
    if (!HasOverlayMultiFrames() || overlayFrames.empty() || !overlayFrames[0]) return;
    int W = vScreenW > 0 ? vScreenW : GetSystemMetrics(SM_CXSCREEN);
    int H = vScreenH > 0 ? vScreenH : GetSystemMetrics(SM_CYSCREEN);
    int maxStripH = std::max(160, (int)(H * 0.48));
    int stripH = std::max(92, std::min(maxStripH, overlayFrameStripHeight));
    int cardH = std::max(54, stripH - 30);
    int thumbH = std::max(32, cardH - 22);
    int origW = (int)overlayFrames[0]->GetWidth();
    int origH = (int)overlayFrames[0]->GetHeight();
    double aspect = (origH > 0) ? ((double)origW / (double)origH) : 1.0;
    int thumbW = std::max(44, std::min(320, (int)std::round(thumbH * aspect)));
    int cardW = thumbW + 12;
    int cardGap = 8;
    int sidePad = 14;
    int cardLeft = sidePad + (int)overlayActiveFrameIdx * (cardW + cardGap);
    int cardRight = cardLeft + cardW;
    if ((float)cardLeft < overlayFrameScrollX + sidePad) {
        overlayFrameScrollX = (float)std::max(0, cardLeft - sidePad);
    } else if ((float)cardRight > overlayFrameScrollX + W - sidePad) {
        overlayFrameScrollX = (float)std::max(0, cardRight - W + sidePad);
    }
}

void PepperSnapDaemon::LayoutOverlayFrameStrip(int screenW, int screenH) {
    overlayFrameCardRects.clear();
    if (!HasOverlayMultiFrames() || screenW <= 0 || screenH <= 0) {
        overlayFrameStripPanelRect = { 0, 0, 0, 0 };
        overlayFrameStripToggleBtnRect = { 0, 0, 0, 0 };
        overlayFrameStripPlayBtnRect = { 0, 0, 0, 0 };
        overlayFrameStripResizeRect = { 0, 0, 0, 0 };
        overlayFrameStripScrollTrackRect = { 0, 0, 0, 0 };
        overlayFrameStripScrollThumbRect = { 0, 0, 0, 0 };
        overlayFrameStripScrollLeftRect = { 0, 0, 0, 0 };
        overlayFrameStripScrollRightRect = { 0, 0, 0, 0 };
        return;
    }

    int maxStripH = std::max(160, (int)(screenH * 0.48));
    overlayFrameStripHeight = std::max(92, std::min(maxStripH, overlayFrameStripHeight));
    int toggleW = 248;
    int toggleH = 26;
    int playW = 30;
    int toggleLeft = (screenW - toggleW) / 2;

    if (overlayFrameStripCollapsed) {
        overlayFrameStripPanelRect = { 0, 0, 0, 0 };
        overlayFrameStripResizeRect = { 0, 0, 0, 0 };
        overlayFrameStripScrollTrackRect = { 0, 0, 0, 0 };
        overlayFrameStripScrollThumbRect = { 0, 0, 0, 0 };
        overlayFrameStripScrollLeftRect = { 0, 0, 0, 0 };
        overlayFrameStripScrollRightRect = { 0, 0, 0, 0 };
        overlayFrameStripToggleBtnRect = { toggleLeft, screenH - toggleH, toggleLeft + toggleW, screenH };
        overlayFrameStripPlayBtnRect = { toggleLeft + toggleW - playW, screenH - toggleH, toggleLeft + toggleW, screenH };
        return;
    }

    int panelTop = screenH - overlayFrameStripHeight;
    overlayFrameStripPanelRect = { 0, panelTop, screenW, screenH };
    overlayFrameStripToggleBtnRect = { toggleLeft, panelTop - toggleH, toggleLeft + toggleW, panelTop };
    overlayFrameStripPlayBtnRect = { toggleLeft + toggleW - playW, panelTop - toggleH, toggleLeft + toggleW, panelTop };
    overlayFrameStripResizeRect = { 0, panelTop - 5, screenW, panelTop + 6 };

    int cardsTop = panelTop + 8;
    int cardsBottom = screenH - 22;
    int cardH = std::max(54, cardsBottom - cardsTop);
    int thumbH = std::max(32, cardH - 22);
    int origW = overlayFrames[0] ? (int)overlayFrames[0]->GetWidth() : 64;
    int origH = overlayFrames[0] ? (int)overlayFrames[0]->GetHeight() : 64;
    double aspect = (origH > 0) ? ((double)origW / (double)origH) : 1.0;
    int thumbW = std::max(44, std::min(320, (int)std::round(thumbH * aspect)));
    int cardW = thumbW + 12;
    int cardGap = 8;
    int sidePad = 14;

    int n = (int)overlayFrames.size();
    int totalFramesW = sidePad * 2 + n * cardW + std::max(0, n - 1) * cardGap;
    overlayFrameMaxScrollX = (float)std::max(0, totalFramesW - screenW);
    overlayFrameScrollX = std::max(0.0f, std::min(overlayFrameMaxScrollX, overlayFrameScrollX));

    overlayFrameCardRects.resize(overlayFrames.size());
    int scrollInt = (int)std::round(overlayFrameScrollX);
    for (int i = 0; i < n; ++i) {
        int cx = sidePad + i * (cardW + cardGap) - scrollInt;
        overlayFrameCardRects[i] = { cx, cardsTop, cx + cardW, cardsTop + cardH };
    }

    int sbTop = screenH - 18;
    int sbBot = screenH - 3;
    overlayFrameStripScrollLeftRect  = { 10, sbTop, 30, sbBot };
    overlayFrameStripScrollRightRect = { screenW - 30, sbTop, screenW - 10, sbBot };
    int trackLeft = 34;
    int trackRight = std::max(trackLeft + 40, screenW - 34);
    overlayFrameStripScrollTrackRect = { trackLeft, sbTop, trackRight, sbBot };

    int trackW = trackRight - trackLeft;
    if (overlayFrameMaxScrollX > 0.5f && totalFramesW > 0) {
        int thumbSpan = std::max(36, std::min(trackW, (int)std::round((double)trackW * (double)screenW / (double)totalFramesW)));
        int thumbX = trackLeft + (int)std::round(((double)overlayFrameScrollX / (double)overlayFrameMaxScrollX) * (trackW - thumbSpan));
        overlayFrameStripScrollThumbRect = { thumbX, sbTop + 1, thumbX + thumbSpan, sbBot - 1 };
    } else {
        overlayFrameStripScrollThumbRect = { trackLeft, sbTop + 1, trackRight, sbBot - 1 };
    }
}

static void FillCheckerboardCanvas(Bitmap* bmp, int rx, int ry, int rw, int rh, int cellSize = 16) {
    if (!bmp || rw <= 0 || rh <= 0) return;
    int bw = (int)bmp->GetWidth();
    int bh = (int)bmp->GetHeight();
    int x0 = std::max(0, rx);
    int y0 = std::max(0, ry);
    int x1 = std::min(bw, rx + rw);
    int y1 = std::min(bh, ry + rh);
    if (x1 <= x0 || y1 <= y0) return;

    cellSize = std::max(4, cellSize);
    Rect lockRc(x0, y0, x1 - x0, y1 - y0);
    BitmapData bd = {};
    if (bmp->LockBits(&lockRc, ImageLockModeWrite, PixelFormat32bppARGB, &bd) == Ok) {
        const DWORD colDark  = 0xFF1E293Bu; // RGB(30, 41, 59)
        const DWORD colLight = 0xFF334155u; // RGB(51, 65, 85)
        int spanW = x1 - x0;
        std::vector<DWORD> rowEven(spanW), rowOdd(spanW);
        for (int dx = 0; dx < spanW; ++dx) {
            int gx = (x0 + dx) / cellSize;
            rowEven[dx] = (gx & 1) ? colLight : colDark;
            rowOdd[dx]  = (gx & 1) ? colDark  : colLight;
        }
        for (int y = y0; y < y1; ++y) {
            DWORD* dstRow = (DWORD*)((BYTE*)bd.Scan0 + (size_t)(y - y0) * bd.Stride);
            const DWORD* srcPattern = (((y / cellSize) & 1) == 0) ? rowEven.data() : rowOdd.data();
            memcpy(dstRow, srcPattern, (size_t)spanW * sizeof(DWORD));
        }
        bmp->UnlockBits(&bd);
    }
}

void PepperSnapDaemon::SelectOverlayFrame(size_t frameIdx) {
    if (frameIdx >= overlayFrames.size() || !overlayFrames[frameIdx] || !frozenDesktopBmp) return;
    overlayActiveFrameIdx = frameIdx;
    Bitmap* frameBmp = overlayFrames[frameIdx];
    int imgW = (int)frameBmp->GetWidth();
    int imgH = (int)frameBmp->GetHeight();
    int drawX = overlayImgDrawRect.left;
    int drawY = overlayImgDrawRect.top;
    int drawW = overlayImgDrawRect.right - overlayImgDrawRect.left;
    int drawH = overlayImgDrawRect.bottom - overlayImgDrawRect.top;
    if (imgW > 0 && imgH > 0 && drawW > 0 && drawH > 0) {
        FillCheckerboardCanvas(frozenDesktopBmp, drawX, drawY, drawW, drawH, 16);
        Graphics g(frozenDesktopBmp);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(PixelOffsetModeHalf);
        ImageAttributes ia;
        ia.SetWrapMode(WrapModeTileFlipXY);
        double fitScale = std::min((double)drawW / (double)imgW, (double)drawH / (double)imgH);
        int fitW = std::max(1, std::min(drawW, (int)std::round(imgW * fitScale)));
        int fitH = std::max(1, std::min(drawH, (int)std::round(imgH * fitScale)));
        int fitX = drawX + (drawW - fitW) / 2;
        int fitY = drawY + (drawH - fitH) / 2;
        g.DrawImage(frameBmp, Rect(fitX, fitY, fitW, fitH), 0, 0, imgW, imgH, UnitPixel, &ia);
        BuildOverlaySurfaceCache();
    }
    if (hOverlayWnd && IsWindow(hOverlayWnd)) {
        InvalidateRect(hOverlayWnd, nullptr, FALSE);
    }
}

void PepperSnapDaemon::StopOverlayFramePlayback() {
    if (!overlayFramesPlaying) return;
    overlayFramesPlaying = false;
    if (hOverlayWnd && IsWindow(hOverlayWnd)) {
        KillTimer(hOverlayWnd, 3);
        InvalidateRect(hOverlayWnd, nullptr, FALSE);
    }
}

void PepperSnapDaemon::ToggleOverlayFramePlayback() {
    if (!HasOverlayMultiFrames() || !hOverlayWnd || !IsWindow(hOverlayWnd)) return;
    overlayFramesPlaying = !overlayFramesPlaying;
    if (overlayFramesPlaying) {
        int delayMs = (overlayActiveFrameIdx < overlayFrameDelaysMs.size()) ? overlayFrameDelaysMs[overlayActiveFrameIdx] : 80;
        delayMs = std::max(20, std::min(60000, delayMs));
        SetTimer(hOverlayWnd, 3, (UINT)delayMs, nullptr);
    } else {
        KillTimer(hOverlayWnd, 3);
    }
    InvalidateRect(hOverlayWnd, nullptr, FALSE);
}

void PepperSnapDaemon::RenderOverlayFrameStrip(Graphics& g, int screenW, int screenH) {
    if (!HasOverlayMultiFrames()) return;
    LayoutOverlayFrameStrip(screenW, screenH);

    FontFamily ff(L"Segoe UI");
    Font btnFont(&ff, 11.5f, FontStyleBold, UnitPixel);
    Font cardFont(&ff, 10.5f, FontStyleBold, UnitPixel);
    StringFormat centerSf;
    centerSf.SetAlignment(StringAlignmentCenter);
    centerSf.SetLineAlignment(StringAlignmentCenter);

    // Render Dropdown Toggle Button + Play/Pause Icon inside the same square (Always visible)
    {
        RECT tr = overlayFrameStripToggleBtnRect;
        RECT playRc = overlayFrameStripPlayBtnRect;
        float playW = (float)(playRc.right - playRc.left);
        RectF trf((float)tr.left, (float)tr.top, (float)(tr.right - tr.left), (float)(tr.bottom - tr.top));
        bool anySqHover = overlayToggleBtnHovered || overlayPlayBtnHovered;
        SolidBrush btnBg(Color(245, 15, 23, 42));
        g.FillRectangle(&btnBg, trf);

        if (overlayToggleBtnHovered) {
            SolidBrush leftHovBg(Color(252, 30, 41, 59));
            g.FillRectangle(&leftHovBg, trf.X, trf.Y, trf.Width - playW, trf.Height);
        }
        if (overlayPlayBtnHovered) {
            SolidBrush rightHovBg(Color(252, 30, 41, 59));
            g.FillRectangle(&rightHovBg, (float)playRc.left, (float)playRc.top, playW, trf.Height);
        } else if (overlayFramesPlaying) {
            SolidBrush activePlayBg(Color(220, 45, 20, 32));
            g.FillRectangle(&activePlayBg, (float)playRc.left, (float)playRc.top, playW, trf.Height);
        }

        // Subtle inner divider separating Hide/Show frames text from the Play/Pause button on the right
        Pen divPen(Color(210, 71, 85, 105), 1.0f);
        g.DrawLine(&divPen, (float)playRc.left, trf.Y + 4.0f, (float)playRc.left, trf.Y + trf.Height - 4.0f);

        // Draw Up/Down Chevron Icon on the left side
        float chevCx = trf.X + 16.0f;
        float chevCy = trf.Y + trf.Height * 0.5f;
        Pen chevPen(Color(255, 239, 68, 68), 2.0f);
        chevPen.SetStartCap(LineCapRound);
        chevPen.SetEndCap(LineCapRound);
        if (overlayFrameStripCollapsed) {
            // Up chevron (click to show frame list)
            g.DrawLine(&chevPen, chevCx - 4.5f, chevCy + 2.0f, chevCx, chevCy - 2.5f);
            g.DrawLine(&chevPen, chevCx, chevCy - 2.5f, chevCx + 4.5f, chevCy + 2.0f);
        } else {
            // Down chevron (click to hide frame list)
            g.DrawLine(&chevPen, chevCx - 4.5f, chevCy - 2.0f, chevCx, chevCy + 2.5f);
            g.DrawLine(&chevPen, chevCx, chevCy + 2.5f, chevCx + 4.5f, chevCy - 2.0f);
        }

        // Draw Label in the center between dropdown chevron and play/pause icon
        std::wstring toggleLbl = (overlayFrameStripCollapsed ? L"Show frames (" : L"Hide frames (") +
                                 std::to_wstring(overlayActiveFrameIdx + 1) + L" / " +
                                 std::to_wstring(overlayFrames.size()) + L")";
        SolidBrush txtBr(Color(255, 248, 250, 252));
        RectF lblRc(trf.X + 26.0f, trf.Y, (trf.Width - playW) - 28.0f, trf.Height);
        g.DrawString(toggleLbl.c_str(), -1, &btnFont, lblRc, &centerSf, &txtBr);

        // Draw Play / Pause Icon on the right side literally opposite the dropdown chevron inside the square
        float pcx = (playRc.left + playRc.right) * 0.5f;
        float pcy = (playRc.top + playRc.bottom) * 0.5f;
        Color iconCol = (overlayPlayBtnHovered || overlayFramesPlaying)
            ? Color(255, 239, 68, 68)
            : Color(255, 248, 250, 252);
        SolidBrush iconBr(iconCol);
        if (overlayFramesPlaying) {
            // Pause icon (two vertical bars)
            g.FillRectangle(&iconBr, pcx - 4.5f, pcy - 5.0f, 3.0f, 10.0f);
            g.FillRectangle(&iconBr, pcx + 1.5f, pcy - 5.0f, 3.0f, 10.0f);
        } else {
            // Play icon (right-pointing triangle)
            PointF tri[3] = {
                PointF(pcx - 3.5f, pcy - 5.5f),
                PointF(pcx - 3.5f, pcy + 5.5f),
                PointF(pcx + 5.5f, pcy)
            };
            g.FillPolygon(&iconBr, tri, 3);
        }

        Pen btnBorder(anySqHover ? Color(255, 239, 68, 68) : Color(230, 71, 85, 105), 1.5f);
        g.DrawRectangle(&btnBorder, trf.X, trf.Y, trf.Width, trf.Height);

        if (anySqHover && dragMode == DragMode::None) {
            DockButton tipBtn;
            if (overlayPlayBtnHovered) {
                tipBtn.rect = playRc;
                tipBtn.tooltip = FormatLabelWithShortcut(overlayFramesPlaying ? L"Pause animation" : L"Play animation", hkFramePlayPause);
            } else {
                tipBtn.rect = { tr.left, tr.top, playRc.left, tr.bottom };
                tipBtn.tooltip = FormatLabelWithShortcut(overlayFrameStripCollapsed ? L"Show frames" : L"Hide frames", hkFrameToggleStrip);
            }
            DrawHoverBubbleTooltip(g, tipBtn, screenW);
        }
    }

    if (overlayFrameStripCollapsed) return;

    // Render Expanded Bottom Frame Strip Panel
    RECT pr = overlayFrameStripPanelRect;
    SolidBrush panelBg(Color(246, 15, 23, 42));
    g.FillRectangle(&panelBg, (INT)pr.left, (INT)pr.top, (INT)(pr.right - pr.left), (INT)(pr.bottom - pr.top));

    bool topResizeActive = (dragMode == DragMode::ResizingFrameStrip) || overlayTopResizeHovered;
    Pen topBorderPen(topResizeActive ? Color(255, 239, 68, 68) : Color(220, 71, 85, 105), topResizeActive ? 2.2f : 1.5f);
    g.DrawLine(&topBorderPen, (INT)0, (INT)pr.top, (INT)screenW, (INT)pr.top);

    // Subtle vertical-resize grip indicators on both sides of the dropdown tab
    Pen gripPen(topResizeActive ? Color(255, 248, 250, 252) : Color(160, 148, 163, 184), 1.2f);
    int leftGripCx = overlayFrameStripToggleBtnRect.left - 36;
    int rightGripCx = overlayFrameStripToggleBtnRect.right + 36;
    if (leftGripCx > 24) {
        g.DrawLine(&gripPen, (INT)(leftGripCx - 14), (INT)(pr.top + 3), (INT)(leftGripCx + 14), (INT)(pr.top + 3));
        g.DrawLine(&gripPen, (INT)(leftGripCx - 14), (INT)(pr.top + 5), (INT)(leftGripCx + 14), (INT)(pr.top + 5));
    }
    if (rightGripCx < screenW - 24) {
        g.DrawLine(&gripPen, (INT)(rightGripCx - 14), (INT)(pr.top + 3), (INT)(rightGripCx + 14), (INT)(pr.top + 3));
        g.DrawLine(&gripPen, (INT)(rightGripCx - 14), (INT)(pr.top + 5), (INT)(rightGripCx + 14), (INT)(pr.top + 5));
    }

    // Render Frame Cards (Clipped horizontally to screen bounds)
    GraphicsState clipSt = g.Save();
    g.SetClip(Rect(0, (INT)(pr.top + 6), screenW, (INT)((pr.bottom - pr.top) - 25)), CombineModeReplace);

    SolidBrush chkDark(Color(255, 30, 41, 59));
    SolidBrush chkLight(Color(255, 51, 65, 85));

    for (size_t i = 0; i < overlayFrameCardRects.size() && i < overlayFrames.size(); ++i) {
        const RECT& cr = overlayFrameCardRects[i];
        if (cr.right < 0 || cr.left > screenW) continue;

        bool isSel = (i == overlayActiveFrameIdx);
        bool isHov = ((int)i == overlayHoveredFrameIdx);
        RectF crf((float)cr.left, (float)cr.top, (float)(cr.right - cr.left), (float)(cr.bottom - cr.top));

        Color cardBgCol = isSel
            ? Color(255, 45, 26, 38)
            : (isHov ? Color(250, 51, 65, 85) : Color(242, 30, 41, 59));
        SolidBrush cardBg(cardBgCol);
        g.FillRectangle(&cardBg, crf);

        int tx = cr.left + 6;
        int ty = cr.top + 5;
        int tw = std::max(8, (int)(cr.right - cr.left) - 12);
        int th = std::max(8, (int)(cr.bottom - cr.top) - 23);

        // Checkerboard backdrop inside thumbnail for transparent GIF/WebP frames
        g.FillRectangle(&chkDark, tx, ty, tw, th);
        const int cell = 8;
        for (int cy = 0; cy < th; cy += cell) {
            int ch = std::min(cell, th - cy);
            for (int cx = 0; cx < tw; cx += cell) {
                if (((cx / cell) + (cy / cell)) & 1) {
                    int cw = std::min(cell, tw - cx);
                    g.FillRectangle(&chkLight, tx + cx, ty + cy, cw, ch);
                }
            }
        }

        if (Bitmap* fb = overlayFrames[i]) {
            int fw = (int)fb->GetWidth();
            int fh = (int)fb->GetHeight();
            if (fw > 0 && fh > 0) {
                double sc = std::min((double)tw / (double)fw, (double)th / (double)fh);
                int dw = std::max(1, (int)std::round(fw * sc));
                int dh = std::max(1, (int)std::round(fh * sc));
                int dx = tx + (tw - dw) / 2;
                int dy = ty + (th - dh) / 2;
                g.DrawImage(fb, Rect(dx, dy, dw, dh), 0, 0, fw, fh, UnitPixel);
            }
        }

        int delayMs = (i < overlayFrameDelaysMs.size()) ? overlayFrameDelaysMs[i] : 100;
        std::wstring cardLbl = (cr.right - cr.left >= 82)
            ? (L"#" + std::to_wstring(i + 1) + L" \x00B7 " + std::to_wstring(delayMs) + L"ms")
            : (L"#" + std::to_wstring(i + 1));
        SolidBrush lblBr(isSel ? Color(255, 254, 202, 202) : Color(255, 226, 232, 240));
        RectF lblRc((float)cr.left + 2.0f, (float)(cr.bottom - 18), (float)(cr.right - cr.left - 4), 17.0f);
        g.DrawString(cardLbl.c_str(), -1, &cardFont, lblRc, &centerSf, &lblBr);

        Color borderCol = isSel
            ? Color(255, 239, 68, 68)
            : (isHov ? Color(255, 56, 189, 248) : Color(210, 71, 85, 105));
        Pen borderPen(borderCol, isSel ? 2.2f : (isHov ? 1.5f : 1.0f));
        g.DrawRectangle(&borderPen, crf.X, crf.Y, crf.Width, crf.Height);
    }

    g.Restore(clipSt);

    // Render Horizontal Scrollbar & Left/Right Step Buttons at Bottom of Strip
    auto drawStepBtn = [&](const RECT& r, bool isLeft, bool isHov) {
        SolidBrush bBg(isHov ? Color(255, 51, 65, 85) : Color(235, 30, 41, 59));
        Pen bPen(Color(200, 71, 85, 105), 1.0f);
        g.FillRectangle(&bBg, (INT)r.left, (INT)r.top, (INT)(r.right - r.left), (INT)(r.bottom - r.top));
        g.DrawRectangle(&bPen, (INT)r.left, (INT)r.top, (INT)(r.right - r.left), (INT)(r.bottom - r.top));
        float cx = (r.left + r.right) * 0.5f;
        float cy = (r.top + r.bottom) * 0.5f;
        Pen arrPen(Color(255, 226, 232, 240), 1.6f);
        if (isLeft) {
            g.DrawLine(&arrPen, cx + 2.0f, cy - 3.5f, cx - 2.0f, cy);
            g.DrawLine(&arrPen, cx - 2.0f, cy, cx + 2.0f, cy + 3.5f);
        } else {
            g.DrawLine(&arrPen, cx - 2.0f, cy - 3.5f, cx + 2.0f, cy);
            g.DrawLine(&arrPen, cx + 2.0f, cy, cx - 2.0f, cy + 3.5f);
        }
    };
    drawStepBtn(overlayFrameStripScrollLeftRect, true, overlayScrollLeftBtnHovered);
    drawStepBtn(overlayFrameStripScrollRightRect, false, overlayScrollRightBtnHovered);

    RECT trk = overlayFrameStripScrollTrackRect;
    SolidBrush trkBg(Color(220, 9, 13, 22));
    Pen trkBorder(Color(180, 51, 65, 85), 1.0f);
    g.FillRectangle(&trkBg, (INT)trk.left, (INT)trk.top, (INT)(trk.right - trk.left), (INT)(trk.bottom - trk.top));
    g.DrawRectangle(&trkBorder, (INT)trk.left, (INT)trk.top, (INT)(trk.right - trk.left), (INT)(trk.bottom - trk.top));

    RECT thm = overlayFrameStripScrollThumbRect;
    bool thumbActive = (dragMode == DragMode::ScrollingFrameStrip) || overlayScrollbarHovered;
    SolidBrush thmBg(thumbActive ? Color(255, 239, 68, 68) : Color(240, 100, 116, 139));
    g.FillRectangle(&thmBg, (INT)thm.left, (INT)thm.top, (INT)(thm.right - thm.left), (INT)(thm.bottom - thm.top));
}

void PepperSnapDaemon::OpenImageFileIntoOverlay(const std::wstring& filePath) {
    if (filePath.empty()) return;
    std::vector<Bitmap*> frames;
    std::vector<int> delays;
    if (!LoadImageFramesFromFile(filePath, frames, delays) || frames.empty()) return;

    if (hOverlayWnd) {
        DiscardSavedEditSession();
        CloseRegionSnipOverlay();
    }
    ClearOverlayFrames();

    Bitmap* firstFrame = frames[0];
    int sw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int sh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (sw <= 0 || sh <= 0) {
        sw = GetSystemMetrics(SM_CXSCREEN);
        sh = GetSystemMetrics(SM_CYSCREEN);
    }

    int imgW = (int)firstFrame->GetWidth();
    int imgH = (int)firstFrame->GetHeight();
    if (imgW <= 0 || imgH <= 0) {
        for (Bitmap* f : frames) delete f;
        return;
    }

    int drawW = imgW;
    int drawH = imgH;
    if (drawW > sw || drawH > sh) {
        double scale = std::min((double)sw / (double)drawW, (double)sh / (double)drawH);
        drawW = std::max(1, (int)std::round(drawW * scale));
        drawH = std::max(1, (int)std::round(drawH * scale));
    }

    int imgX = (sw - drawW) / 2;
    int imgY = (sh - drawH) / 2;

    Bitmap* canvasBmp = new Bitmap(sw, sh, PixelFormat32bppARGB);
    FillCheckerboardCanvas(canvasBmp, 0, 0, sw, sh, 16);
    Graphics g(canvasBmp);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    ImageAttributes ia;
    ia.SetWrapMode(WrapModeTileFlipXY);
    g.DrawImage(firstFrame, Rect(imgX, imgY, drawW, drawH), 0, 0, imgW, imgH, UnitPixel, &ia);

    overlayImgDrawRect = { imgX, imgY, imgX + drawW, imgY + drawH };
    if (frames.size() > 1) {
        overlayFrames = std::move(frames);
        overlayFrameDelaysMs = std::move(delays);
        overlayActiveFrameIdx = 0;
        overlayFrameStripCollapsed = false;
        overlayFrameScrollX = 0.0f;
        LayoutOverlayFrameStrip(sw, sh);
    } else {
        overlaySingleImageBmp = firstFrame;
    }

    RECT centeredSel = { imgX, imgY, imgX + drawW, imgY + drawH };
    StartRegionSnipOverlay(canvasBmp, &centeredSel);
}

void PepperSnapDaemon::OpenImageIntoOverlay() {
    std::vector<WCHAR> szFile(4096, L'\0');
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hTrayWnd;
    ofn.lpstrFile = szFile.data();
    ofn.nMaxFile = (DWORD)szFile.size();
    ofn.lpstrFilter = L"Image files (*.png;*.jpg;*.jpeg;*.webp;*.bmp;*.gif;*.tif;*.tiff;*.ico)\0*.png;*.jpg;*.jpeg;*.webp;*.bmp;*.gif;*.tif;*.tiff;*.ico\0All files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        OpenImageFileIntoOverlay(szFile.data());
    }
}

static RECT GetPinnedSlotScreenBounds(int imgX, int imgY, int dispW, int dispH, bool hideToolbar, const RECT& rcScreen) {
    const int stripW = 242;
    const int totalHudH = 28 * 2 + 3;
    const int hudGap = 8;
    int sImgL = imgX;
    int sImgT = imgY;
    int sImgR = sImgL + dispW;
    int sImgB = sImgT + dispH;
    if (hideToolbar) {
        return { sImgL, sImgT, sImgR, sImgB };
    }
    int sStripR = std::max((int)rcScreen.left + stripW + 6, std::min((int)rcScreen.right - 6, sImgR));
    int sStripT = sImgB + hudGap;
    bool hasSpaceBelow = (sImgB + hudGap + totalHudH <= rcScreen.bottom - 6) && (sImgB + hudGap >= rcScreen.top + 6);
    bool hasSpaceAbove = (sImgT - hudGap - totalHudH >= rcScreen.top + 6) && (sImgT - hudGap <= rcScreen.bottom - 6);
    if (hasSpaceBelow) {
        sStripT = sImgB + hudGap;
    } else if (hasSpaceAbove) {
        sStripT = sImgT - hudGap - totalHudH;
    } else {
        sStripR = std::max((int)rcScreen.left + stripW + 6, std::min((int)rcScreen.right - 6, sImgR - 8));
        sStripT = std::max((int)rcScreen.top + 6, std::min((int)rcScreen.bottom - totalHudH - 6, sImgB - totalHudH - 8));
    }
    int sStripL = sStripR - stripW;
    int sStripB = sStripT + totalHudH;
    return {
        std::min(sImgL, sStripL),
        std::min(sImgT, sStripT),
        std::max(sImgR, sStripR),
        std::max(sImgB, sStripB)
    };
}

static void FindNonOverlappingPinPosition(
    const std::vector<HWND>& existingPins,
    int dispW,
    int dispH,
    bool hideToolbar,
    const RECT& rcScreen,
    int& outImgX,
    int& outImgY
) {
    const int stripW = 242;
    const int hudExtraH = hideToolbar ? 0 : (8 + 28 * 2 + 3);
    const int pad = 14;

    int leftOverhang = hideToolbar ? 0 : std::max(0, stripW - dispW);
    int minImgX = rcScreen.left + 12 + leftOverhang;
    int maxImgX = std::max(minImgX, (int)rcScreen.right - 12 - dispW);
    int minImgY = rcScreen.top + 12;
    int maxImgY = std::max(minImgY, (int)rcScreen.bottom - 12 - dispH - hudExtraH);

    int centerImgX = std::max(minImgX, std::min(maxImgX, (int)(rcScreen.left + ((rcScreen.right - rcScreen.left) - dispW) / 2)));
    int centerImgY = std::max(minImgY, std::min(maxImgY, (int)(rcScreen.top + ((rcScreen.bottom - rcScreen.top) - (dispH + hudExtraH)) / 2)));

    std::vector<RECT> occupied;
    for (HWND hp : existingPins) {
        if (!hp || !IsWindow(hp)) continue;
        PinnedWindowData* pData = (PinnedWindowData*)GetWindowLongPtrW(hp, GWLP_USERDATA);
        if (!pData || !pData->bmp) continue;
        int pW = 32, pH = 32;
        GetPinnedScaledDims(pData, pW, pH);
        RECT r = GetPinnedSlotScreenBounds(pData->screenImgX, pData->screenImgY, pW, pH, pData->hideToolbar, rcScreen);
        r.left   -= pad;
        r.top    -= pad;
        r.right  += pad;
        r.bottom += pad;
        occupied.push_back(r);
    }

    if (occupied.empty()) {
        outImgX = centerImgX;
        outImgY = centerImgY;
        return;
    }

    auto calcOverlapArea = [&](int candX, int candY) -> long long {
        RECT cand = GetPinnedSlotScreenBounds(candX, candY, dispW, dispH, hideToolbar, rcScreen);
        long long totalOverlap = 0;
        for (const RECT& occ : occupied) {
            RECT inter = {0};
            if (IntersectRect(&inter, &cand, &occ)) {
                totalOverlap += (long long)(inter.right - inter.left) * (long long)(inter.bottom - inter.top);
            }
        }
        return totalOverlap;
    };

    // Try smart relative positions adjacent to existing pinned windows (right, left, below, above)
    std::vector<POINT> candidates;
    candidates.push_back({ centerImgX, centerImgY });
    for (auto it = existingPins.rbegin(); it != existingPins.rend(); ++it) {
        HWND hp = *it;
        if (!hp || !IsWindow(hp)) continue;
        PinnedWindowData* pData = (PinnedWindowData*)GetWindowLongPtrW(hp, GWLP_USERDATA);
        if (!pData || !pData->bmp) continue;
        int pW = 32, pH = 32;
        GetPinnedScaledDims(pData, pW, pH);
        RECT pb = GetPinnedSlotScreenBounds(pData->screenImgX, pData->screenImgY, pW, pH, pData->hideToolbar, rcScreen);
        int rightX = pb.right + pad + leftOverhang;
        int leftX  = pb.left - pad - dispW;
        int belowY = pb.bottom + pad;
        int aboveY = pb.top - pad - dispH - hudExtraH;
        if (rightX >= minImgX && rightX <= maxImgX) {
            candidates.push_back({ rightX, std::max(minImgY, std::min(maxImgY, pData->screenImgY)) });
        }
        if (leftX >= minImgX && leftX <= maxImgX) {
            candidates.push_back({ leftX, std::max(minImgY, std::min(maxImgY, pData->screenImgY)) });
        }
        if (belowY >= minImgY && belowY <= maxImgY) {
            candidates.push_back({ std::max(minImgX, std::min(maxImgX, pData->screenImgX)), belowY });
        }
        if (aboveY >= minImgY && aboveY <= maxImgY) {
            candidates.push_back({ std::max(minImgX, std::min(maxImgX, pData->screenImgX)), aboveY });
        }
    }

    for (const POINT& pt : candidates) {
        if (calcOverlapArea(pt.x, pt.y) == 0) {
            outImgX = pt.x;
            outImgY = pt.y;
            return;
        }
    }

    // Scan work area grid for a zero-overlap spot (or minimum overlap if screen is completely packed)
    int bestX = centerImgX;
    int bestY = centerImgY;
    long long bestOverlap = calcOverlapArea(bestX, bestY);
    const int step = 20;
    for (int y = minImgY; y <= maxImgY; y += step) {
        for (int x = minImgX; x <= maxImgX; x += step) {
            long long ov = calcOverlapArea(x, y);
            if (ov == 0) {
                outImgX = x;
                outImgY = y;
                return;
            }
            if (ov < bestOverlap) {
                bestOverlap = ov;
                bestX = x;
                bestY = y;
            }
        }
    }
    outImgX = bestX;
    outImgY = bestY;
}

void PepperSnapDaemon::OpenImageFilesToPinOnTop(const std::vector<std::wstring>& filePaths) {
    struct LoadedPinItem {
        std::vector<Bitmap*> frames;
        std::vector<int> delays;
        int origW = 0;
        int origH = 0;
        float scale = 1.0f;
        int dispW = 32;
        int dispH = 32;
    };

    std::vector<LoadedPinItem> items;
    for (const auto& path : filePaths) {
        if (path.empty()) continue;
        LoadedPinItem item;
        if (LoadImageFramesFromFile(path, item.frames, item.delays) && !item.frames.empty() && item.frames[0]) {
            item.origW = (int)item.frames[0]->GetWidth();
            item.origH = (int)item.frames[0]->GetHeight();
            if (item.origW > 0 && item.origH > 0) {
                items.push_back(std::move(item));
            } else {
                for (Bitmap* f : item.frames) delete f;
            }
        }
    }
    if (items.empty()) return;

    POINT ptCur = { 0, 0 };
    GetCursorPos(&ptCur);
    HMONITOR hMon = MonitorFromPoint(ptCur, MONITOR_DEFAULTTOPRIMARY);
    RECT rcScreen = GetPinnedMonitorWorkArea(hMon);
    MONITORINFO mi = { sizeof(mi) };
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    if (hMon && GetMonitorInfoW(hMon, &mi)) {
        sw = std::max(1, (int)(mi.rcMonitor.right - mi.rcMonitor.left));
        sh = std::max(1, (int)(mi.rcMonitor.bottom - mi.rcMonitor.top));
    }

    // Limit pinned image size on open to 0.4 of screen size, fitting proportionally so aspect ratio is never distorted
    int maxOpenW = std::max(64, (int)std::round(sw * 0.40));
    int maxOpenH = std::max(64, (int)std::round(sh * 0.40));

    int n = (int)items.size();
    bool selectByDefault = (n > 1);
    if (selectByDefault) {
        ClearAllSelectedPinnedWindows();
    }

    bool hasExistingPins = false;
    for (HWND hp : pinnedWindows) {
        if (hp && IsWindow(hp)) {
            hasExistingPins = true;
            break;
        }
    }

    const int stripW = 242;
    const int hudExtraH = hidePinnedToolbar ? 0 : (8 + 28 * 2 + 3);
    const int gap = 16;

    if (n > 1 && !hasExistingPins) {
        int cols = (int)std::ceil(std::sqrt((double)n));
        if (n == 2) cols = 2;
        else if (n == 3) cols = 3;
        int rows = (n + cols - 1) / cols;

        int workW = std::max(320, (int)(rcScreen.right - rcScreen.left) - 24);
        int workH = std::max(240, (int)(rcScreen.bottom - rcScreen.top) - 24);
        if (cols == 3 && workW < cols * (stripW + gap)) {
            cols = 2;
            rows = (n + cols - 1) / cols;
        }

        int cellMaxW = std::min(maxOpenW, std::max(64, (workW - (cols - 1) * gap) / cols));
        int cellMaxH = std::min(maxOpenH, std::max(64, (workH - (rows - 1) * gap) / rows - hudExtraH));

        for (auto& it : items) {
            float sc = 1.0f;
            if (it.origW > cellMaxW || it.origH > cellMaxH) {
                sc = (float)std::min((double)cellMaxW / (double)it.origW, (double)cellMaxH / (double)it.origH);
            }
            sc = std::max(0.01f, sc);
            it.scale = sc;
            it.dispW = std::max(1, (int)std::round(it.origW * sc));
            it.dispH = std::max(1, (int)std::round(it.origH * sc));
        }

        std::vector<int> rowHeights(rows, 0);
        std::vector<int> rowWidths(rows, 0);
        for (int r = 0; r < rows; ++r) {
            int firstIdx = r * cols;
            int lastIdx = std::min(n, firstIdx + cols);
            int rW = 0;
            int rH = 0;
            for (int idx = firstIdx; idx < lastIdx; ++idx) {
                int slotW = hidePinnedToolbar ? items[idx].dispW : std::max(items[idx].dispW, stripW);
                int slotH = items[idx].dispH + hudExtraH;
                rW += slotW + (idx > firstIdx ? gap : 0);
                rH = std::max(rH, slotH);
            }
            rowWidths[r] = rW;
            rowHeights[r] = rH;
        }

        int totalGridH = 0;
        for (int r = 0; r < rows; ++r) {
            totalGridH += rowHeights[r] + (r > 0 ? gap : 0);
        }

        int curY = std::max((int)rcScreen.top + 12, (int)(rcScreen.top + ((rcScreen.bottom - rcScreen.top) - totalGridH) / 2));
        for (int r = 0; r < rows; ++r) {
            int firstIdx = r * cols;
            int lastIdx = std::min(n, firstIdx + cols);
            int curX = std::max((int)rcScreen.left + 12, (int)(rcScreen.left + ((rcScreen.right - rcScreen.left) - rowWidths[r]) / 2));
            for (int idx = firstIdx; idx < lastIdx; ++idx) {
                auto& it = items[idx];
                int slotW = hidePinnedToolbar ? it.dispW : std::max(it.dispW, stripW);
                // When dispW < stripW, the toolbar extends to the left of the image's right edge
                int imgX = curX + (slotW - it.dispW);
                int imgY = curY + (rowHeights[r] - hudExtraH - it.dispH) / 2;
                Bitmap* firstBmp = it.frames[0];
                if (it.frames.size() > 1) {
                    CreatePinnedWindow(firstBmp, imgX, imgY, it.scale, std::move(it.frames), std::move(it.delays), selectByDefault);
                } else {
                    CreatePinnedWindow(firstBmp, imgX, imgY, it.scale, {}, {}, selectByDefault);
                }
                curX += slotW + gap;
            }
            curY += rowHeights[r] + gap;
        }
        return;
    }

    for (auto& it : items) {
        float initScale = 1.0f;
        if (it.origW > maxOpenW || it.origH > maxOpenH) {
            initScale = (float)std::min((double)maxOpenW / (double)it.origW, (double)maxOpenH / (double)it.origH);
        }
        initScale = std::max(0.01f, initScale);
        it.scale = initScale;
        it.dispW = std::max(1, (int)std::round(it.origW * initScale));
        it.dispH = std::max(1, (int)std::round(it.origH * initScale));

        int px = 0, py = 0;
        FindNonOverlappingPinPosition(pinnedWindows, it.dispW, it.dispH, hidePinnedToolbar, rcScreen, px, py);

        Bitmap* firstBmp = it.frames[0];
        if (it.frames.size() > 1) {
            CreatePinnedWindow(firstBmp, px, py, it.scale, std::move(it.frames), std::move(it.delays), selectByDefault);
        } else {
            CreatePinnedWindow(firstBmp, px, py, it.scale, {}, {}, selectByDefault);
        }
    }
}

void PepperSnapDaemon::OpenImageToPinOnTop() {
    std::vector<WCHAR> szFileBuf(65536, 0);
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hTrayWnd;
    ofn.lpstrFile = szFileBuf.data();
    ofn.nMaxFile = (DWORD)szFileBuf.size();
    ofn.lpstrFilter = L"Image files (*.png;*.jpg;*.jpeg;*.webp;*.bmp;*.gif;*.tif;*.tiff;*.ico)\0*.png;*.jpg;*.jpeg;*.webp;*.bmp;*.gif;*.tif;*.tiff;*.ico\0All files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

    if (GetOpenFileNameW(&ofn)) {
        std::vector<std::wstring> selectedFiles;
        const WCHAR* p = szFileBuf.data();
        std::wstring firstPart(p);
        p += firstPart.size() + 1;
        if (*p == L'\0') {
            if (!firstPart.empty()) selectedFiles.push_back(firstPart);
        } else {
            std::wstring dir = firstPart;
            if (!dir.empty() && dir.back() != L'\\' && dir.back() != L'/') {
                dir += L"\\";
            }
            while (*p != L'\0') {
                std::wstring fileName(p);
                p += fileName.size() + 1;
                if (!fileName.empty()) {
                    selectedFiles.push_back(dir + fileName);
                }
            }
        }
        if (!selectedFiles.empty()) {
            OpenImageFilesToPinOnTop(selectedFiles);
        }
    }
}

static Bitmap* ExtractClipboardBitmap(HWND hWndOwner) {
    Bitmap* clipBmp = nullptr;
    if (OpenClipboard(hWndOwner)) {
        UINT cfPng = RegisterClipboardFormatW(L"PNG");
        if (cfPng && IsClipboardFormatAvailable(cfPng)) {
            HGLOBAL hPng = GetClipboardData(cfPng);
            if (hPng) {
                SIZE_T sz = GlobalSize(hPng);
                void* pData = GlobalLock(hPng);
                if (pData && sz > 0) {
                    HGLOBAL hCopy = GlobalAlloc(GMEM_MOVEABLE, sz);
                    if (hCopy) {
                        void* pCopy = GlobalLock(hCopy);
                        if (pCopy) {
                            memcpy(pCopy, pData, sz);
                            GlobalUnlock(hCopy);
                            IStream* pStream = nullptr;
                            if (SUCCEEDED(CreateStreamOnHGlobal(hCopy, TRUE, &pStream)) && pStream) {
                                Bitmap* tmp = Bitmap::FromStream(pStream);
                                if (tmp && tmp->GetLastStatus() == Ok && tmp->GetWidth() > 0 && tmp->GetHeight() > 0) {
                                    int w = (int)tmp->GetWidth();
                                    int h = (int)tmp->GetHeight();
                                    clipBmp = new Bitmap(w, h, PixelFormat32bppPARGB);
                                    Graphics g(clipBmp);
                                    g.SetCompositingMode(CompositingModeSourceCopy);
                                    g.DrawImage(tmp, 0, 0, w, h);
                                }
                                delete tmp;
                                pStream->Release();
                            } else {
                                GlobalFree(hCopy);
                            }
                        } else {
                            GlobalFree(hCopy);
                        }
                    }
                }
                if (pData) GlobalUnlock(hPng);
            }
        }

        if (!clipBmp && (IsClipboardFormatAvailable(CF_DIBV5) || IsClipboardFormatAvailable(CF_DIB))) {
            UINT fmt = IsClipboardFormatAvailable(CF_DIBV5) ? CF_DIBV5 : CF_DIB;
            HGLOBAL hDib = GetClipboardData(fmt);
            if (hDib) {
                SIZE_T dibSize = GlobalSize(hDib);
                BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)GlobalLock(hDib);
                if (bih && dibSize >= sizeof(BITMAPINFOHEADER) && bih->biWidth > 0 && bih->biHeight != 0) {
                    int w = (int)bih->biWidth;
                    int h = std::abs((int)bih->biHeight);
                    bool topDown = (bih->biHeight < 0);
                    if (bih->biBitCount == 32 && (bih->biCompression == BI_RGB || bih->biCompression == BI_BITFIELDS)) {
                        DWORD headerBytes = bih->biSize;
                        if (bih->biSize == sizeof(BITMAPINFOHEADER) && bih->biCompression == BI_BITFIELDS) {
                            headerBytes += 12;
                        }
                        if (headerBytes + (SIZE_T)w * (SIZE_T)h * 4 <= dibSize) {
                            const BYTE* srcBase = (const BYTE*)bih + headerBytes;
                            bool anyAlpha = false;
                            for (int i = 0; i < w * h; ++i) {
                                if (srcBase[i * 4 + 3] != 0) {
                                    anyAlpha = true;
                                    break;
                                }
                            }
                            clipBmp = new Bitmap(w, h, PixelFormat32bppPARGB);
                            BitmapData bd = {0};
                            Rect r(0, 0, w, h);
                            if (clipBmp->LockBits(&r, ImageLockModeWrite, PixelFormat32bppPARGB, &bd) == Ok) {
                                for (int y = 0; y < h; ++y) {
                                    int srcY = topDown ? y : (h - 1 - y);
                                    const BYTE* srcRow = srcBase + (size_t)srcY * (size_t)w * 4;
                                    BYTE* dstRow = (BYTE*)bd.Scan0 + (size_t)y * (size_t)bd.Stride;
                                    for (int x = 0; x < w; ++x) {
                                        BYTE b = srcRow[x * 4 + 0];
                                        BYTE g = srcRow[x * 4 + 1];
                                        BYTE rC = srcRow[x * 4 + 2];
                                        BYTE a = anyAlpha ? srcRow[x * 4 + 3] : 255;
                                        dstRow[x * 4 + 0] = (BYTE)((b * a + 127) / 255);
                                        dstRow[x * 4 + 1] = (BYTE)((g * a + 127) / 255);
                                        dstRow[x * 4 + 2] = (BYTE)((rC * a + 127) / 255);
                                        dstRow[x * 4 + 3] = a;
                                    }
                                }
                                clipBmp->UnlockBits(&bd);
                            }
                        }
                    }
                }
                if (bih) GlobalUnlock(hDib);
            }
        }

        if (!clipBmp && IsClipboardFormatAvailable(CF_BITMAP)) {
            HBITMAP hBmp = (HBITMAP)GetClipboardData(CF_BITMAP);
            HPALETTE hPal = IsClipboardFormatAvailable(CF_PALETTE) ? (HPALETTE)GetClipboardData(CF_PALETTE) : nullptr;
            if (hBmp) {
                Bitmap* tmp = Bitmap::FromHBITMAP(hBmp, hPal);
                if (tmp && tmp->GetLastStatus() == Ok && tmp->GetWidth() > 0 && tmp->GetHeight() > 0) {
                    int w = (int)tmp->GetWidth();
                    int h = (int)tmp->GetHeight();
                    clipBmp = new Bitmap(w, h, PixelFormat32bppPARGB);
                    Graphics g(clipBmp);
                    g.SetCompositingMode(CompositingModeSourceCopy);
                    g.DrawImage(tmp, 0, 0, w, h);
                }
                delete tmp;
            }
        }

        CloseClipboard();
    }
    return clipBmp;
}

void PepperSnapDaemon::PinClipboardImageOnTop() {
    if (IsClipboardFormatAvailable(CF_HDROP)) {
        if (OpenClipboard(hTrayWnd)) {
            std::vector<std::wstring> droppedFiles;
            HDROP hDrop = (HDROP)GetClipboardData(CF_HDROP);
            if (hDrop) {
                UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
                for (UINT i = 0; i < count; ++i) {
                    UINT len = DragQueryFileW(hDrop, i, nullptr, 0);
                    if (len > 0) {
                        std::wstring path(len, L'\0');
                        DragQueryFileW(hDrop, i, &path[0], len + 1);
                        droppedFiles.push_back(path);
                    }
                }
            }
            CloseClipboard();
            if (!droppedFiles.empty()) {
                size_t beforeCount = pinnedWindows.size();
                OpenImageFilesToPinOnTop(droppedFiles);
                if (pinnedWindows.size() > beforeCount) {
                    return;
                }
            }
        }
    }

    Bitmap* clipBmp = ExtractClipboardBitmap(hTrayWnd);
    if (!clipBmp) {
        return;
    }

    POINT pt = {0, 0};
    GetCursorPos(&pt);
    HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    RECT rcScreen = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    if (GetMonitorInfoW(hMon, &mi)) {
        rcScreen = mi.rcWork;
    }

    int maxOpenW = std::max(120, (int)((rcScreen.right - rcScreen.left) * 85 / 100));
    int maxOpenH = std::max(120, (int)((rcScreen.bottom - rcScreen.top) * 85 / 100));
    int origW = std::max(1, (int)clipBmp->GetWidth());
    int origH = std::max(1, (int)clipBmp->GetHeight());

    float initScale = 1.0f;
    if (origW > maxOpenW || origH > maxOpenH) {
        initScale = (float)std::min((double)maxOpenW / (double)origW, (double)maxOpenH / (double)origH);
    }
    initScale = std::max(0.01f, initScale);
    int dispW = std::max(1, (int)std::round(origW * initScale));
    int dispH = std::max(1, (int)std::round(origH * initScale));

    int px = 0, py = 0;
    FindNonOverlappingPinPosition(pinnedWindows, dispW, dispH, hidePinnedToolbar, rcScreen, px, py);
    CreatePinnedWindow(clipBmp, px, py, initScale);
}

void PepperSnapDaemon::EditClipboardImage() {
    if (IsClipboardFormatAvailable(CF_HDROP)) {
        std::wstring droppedPath;
        if (OpenClipboard(hTrayWnd)) {
            HDROP hDrop = (HDROP)GetClipboardData(CF_HDROP);
            if (hDrop) {
                UINT len = DragQueryFileW(hDrop, 0, nullptr, 0);
                if (len > 0) {
                    droppedPath.resize(len);
                    DragQueryFileW(hDrop, 0, &droppedPath[0], len + 1);
                }
            }
            CloseClipboard();
        }
        if (!droppedPath.empty()) {
            std::wstring resolvedPath;
            if (IsValidExistingImageFilePath(droppedPath, &resolvedPath)) {
                OpenImageFileIntoOverlay(resolvedPath);
                return;
            }
        }
    }

    Bitmap* clipBmp = ExtractClipboardBitmap(hTrayWnd);
    if (!clipBmp) {
        ShowTrayToast(
            L"No image found in clipboard",
            L"Copy an image or image file first, then use " + FormatHotkeyString(hkEditClipboard) + L" or tray menu to edit."
        );
        return;
    }

    if (hOverlayWnd && IsWindow(hOverlayWnd)) {
        CloseRegionSnipOverlay();
    }

    int sw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int sh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (sw <= 0 || sh <= 0) {
        sw = GetSystemMetrics(SM_CXSCREEN);
        sh = GetSystemMetrics(SM_CYSCREEN);
    }

    int imgW = (int)clipBmp->GetWidth();
    int imgH = (int)clipBmp->GetHeight();
    if (imgW <= 0 || imgH <= 0) {
        delete clipBmp;
        return;
    }

    int drawW = imgW;
    int drawH = imgH;
    if (drawW > sw || drawH > sh) {
        double scale = std::min((double)sw / (double)drawW, (double)sh / (double)drawH);
        drawW = std::max(1, (int)std::round(drawW * scale));
        drawH = std::max(1, (int)std::round(drawH * scale));
    }

    int imgX = (sw - drawW) / 2;
    int imgY = (sh - drawH) / 2;

    Bitmap* canvasBmp = new Bitmap(sw, sh, PixelFormat32bppARGB);
    FillCheckerboardCanvas(canvasBmp, 0, 0, sw, sh, 16);
    Graphics g(canvasBmp);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    ImageAttributes ia;
    ia.SetWrapMode(WrapModeTileFlipXY);
    g.DrawImage(clipBmp, Rect(imgX, imgY, drawW, drawH), 0, 0, imgW, imgH, UnitPixel, &ia);

    overlayImgDrawRect = { imgX, imgY, imgX + drawW, imgY + drawH };
    overlaySingleImageBmp = clipBmp;

    RECT centeredSel = { imgX, imgY, imgX + drawW, imgY + drawH };
    StartRegionSnipOverlay(canvasBmp, &centeredSel);
}

// --- "Search on web" --------------------------------------------------------
//
// The image goes on the clipboard and the browser is opened at the Lens URL; the
// user finishes with a single Ctrl+V once the page is up. That keystroke cannot
// land in the wrong window, which is the whole point of not sending it for them.
//
// This was automated first -- find the browser, wait for the page, inject Ctrl+V
// -- and it was measured on a real machine (Opera 137, Chromium) before being
// pulled back out, because none of the signals it needs are dependable:
//
//  * https://lens.google.com/ answers 302 -> https://www.google.com?olud, so the
//    Lens upload panel lives inside the Google home page and the title becomes
//    "Google", never anything containing "lens". There is no title to wait for.
//  * A URL handed to an already-running browser opens a tab: no new top-level
//    window ever appears, and the pid ShellExecute returns belongs to a launcher
//    that exits at once. Window titles cannot identify a browser (they are the
//    active tab's title), and neither can the class, since "Chrome_WidgetWin_1"
//    belongs to every Electron app too.
//  * An injected Ctrl+V goes to whatever holds keyboard focus, in whichever
//    window the shell happened to raise -- seconds after the click on a cold
//    profile. UI Automation can report when focus reaches the page, but that
//    only estimates readiness, and an estimate that is wrong types into an
//    unrelated application.
//
// Opening the page is what the README promises ("simply opens Google Lens in
// your own browser"), and it needs none of that: the toast says the image is on
// the clipboard, and one Ctrl+V finishes the job.
//
// Shift + click on the same button opens Translate instead of Lens. Both are
// only opened; the clipboard handoff and the Ctrl+V are the user's either way.

void PepperSnapDaemon::OpenGoogleLensInBrowser() {
    ShellExecuteW(nullptr, L"open", L"https://lens.google.com/", nullptr, nullptr, SW_SHOWNORMAL);
}

void PepperSnapDaemon::OpenGoogleTranslateInBrowser(const std::wstring& targetLang) {
    // "op=images" is the image-translation view -- the one with the upload panel
    // instead of the two text boxes -- and "sl=auto" leaves the source language
    // to Google. "tl" is the code picked in Options; it comes from the same
    // table that fills that dropdown, so it is always one this URL accepts.
    std::wstring url = L"https://translate.google.com/?sl=auto&tl=" + targetLang + L"&op=images";
    ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

bool IsValidExistingImageFilePath(const std::wstring& rawPath, std::wstring* outResolvedPath) {
    if (rawPath.empty()) return false;
    for (wchar_t ch : rawPath) {
        if (ch < 32) return false;
    }
    DWORD reqLen = GetFullPathNameW(rawPath.c_str(), 0, nullptr, nullptr);
    if (reqLen == 0 || reqLen > 32768) return false;
    std::wstring fullPath(reqLen, L'\0');
    DWORD gotLen = GetFullPathNameW(rawPath.c_str(), reqLen, &fullPath[0], nullptr);
    if (gotLen == 0 || gotLen >= reqLen) return false;
    fullPath.resize(gotLen);

    DWORD attrs = GetFileAttributesW(fullPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) return false;
    if ((attrs & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_REPARSE_POINT)) != 0) return false;

    const wchar_t* ext = PathFindExtensionW(fullPath.c_str());
    if (!ext || ext[0] != L'.') return false;
    static const wchar_t* const kAllowedExts[] = {
        L".png", L".jpg", L".jpeg", L".webp", L".bmp", L".gif", L".tif", L".tiff", L".ico"
    };
    bool extAllowed = false;
    for (const wchar_t* allowed : kAllowedExts) {
        if (_wcsicmp(ext, allowed) == 0) {
            extAllowed = true;
            break;
        }
    }
    if (!extAllowed) return false;

    HANDLE hFile = CreateFileW(fullPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER fileSize = {};
    BYTE hdr[12] = {};
    DWORD bytesRead = 0;
    bool okSize = GetFileSizeEx(hFile, &fileSize) && fileSize.QuadPart >= 4 && fileSize.QuadPart <= (256LL * 1024LL * 1024LL);
    bool okRead = okSize && ReadFile(hFile, hdr, sizeof(hdr), &bytesRead, nullptr) && bytesRead >= 4;
    CloseHandle(hFile);
    if (!okRead) return false;

    bool headerValid = false;
    if (bytesRead >= 8 && hdr[0] == 0x89 && hdr[1] == 'P' && hdr[2] == 'N' && hdr[3] == 'G' &&
        hdr[4] == 0x0D && hdr[5] == 0x0A && hdr[6] == 0x1A && hdr[7] == 0x0A) {
        headerValid = true; // PNG
    } else if (bytesRead >= 3 && hdr[0] == 0xFF && hdr[1] == 0xD8 && hdr[2] == 0xFF) {
        headerValid = true; // JPEG
    } else if (bytesRead >= 12 && hdr[0] == 'R' && hdr[1] == 'I' && hdr[2] == 'F' && hdr[3] == 'F' &&
               hdr[8] == 'W' && hdr[9] == 'E' && hdr[10] == 'B' && hdr[11] == 'P') {
        headerValid = true; // WebP
    } else if (bytesRead >= 2 && hdr[0] == 'B' && hdr[1] == 'M') {
        headerValid = true; // BMP
    } else if (bytesRead >= 6 && hdr[0] == 'G' && hdr[1] == 'I' && hdr[2] == 'F' && hdr[3] == '8' &&
               (hdr[4] == '7' || hdr[4] == '9') && hdr[5] == 'a') {
        headerValid = true; // GIF
    } else if (bytesRead >= 4 && ((hdr[0] == 'I' && hdr[1] == 'I' && hdr[2] == 42 && hdr[3] == 0) ||
                                  (hdr[0] == 'M' && hdr[1] == 'M' && hdr[2] == 0 && hdr[3] == 42))) {
        headerValid = true; // TIFF
    } else if (bytesRead >= 4 && hdr[0] == 0 && hdr[1] == 0 && (hdr[2] == 1 || hdr[2] == 2) && hdr[3] == 0) {
        headerValid = true; // ICO / CUR
    }
    if (!headerValid) return false;

    if (outResolvedPath) *outResolvedPath = fullPath;
    return true;
}

void PepperSnapDaemon::SearchImageFileOnGoogle(const std::wstring& filePath) {
    if (!enableWebImageSearch) return;
    std::wstring resolvedPath;
    if (!IsValidExistingImageFilePath(filePath, &resolvedPath)) return;
    std::vector<Bitmap*> frames;
    std::vector<int> delays;
    if (LoadImageFramesFromFile(resolvedPath, frames, delays) && !frames.empty() && frames[0]) {
        CopyBitmapToClipboard(frames[0]);
        for (Bitmap* f : frames) delete f;
        SearchClipboardImageOnGoogle();
    } else {
        for (Bitmap* f : frames) delete f;
    }
}

void PepperSnapDaemon::SearchClipboardImageOnGoogle(bool translateToTarget) {
    if (hOverlayWnd && IsWindow(hOverlayWnd)) {
        if (!IsEditingOpenedImage()) {
            CloseRegionSnipOverlay();
        }
    }

    UINT cfPng = RegisterClipboardFormatW(L"PNG");
    bool hasDirectClipImage =
        (cfPng && IsClipboardFormatAvailable(cfPng)) ||
        IsClipboardFormatAvailable(CF_DIBV5) ||
        IsClipboardFormatAvailable(CF_DIB);

    if (!hasDirectClipImage && IsClipboardFormatAvailable(CF_HDROP)) {
        std::wstring firstFilePath;
        if (OpenClipboard(hTrayWnd)) {
            HDROP hDrop = (HDROP)GetClipboardData(CF_HDROP);
            if (hDrop) {
                UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
                for (UINT i = 0; i < count; ++i) {
                    UINT len = DragQueryFileW(hDrop, i, nullptr, 0);
                    if (len > 0) {
                        std::wstring path(len, L'\0');
                        DragQueryFileW(hDrop, i, &path[0], len + 1);
                        firstFilePath = path;
                        break;
                    }
                }
            }
            CloseClipboard();
        }
        if (!firstFilePath.empty()) {
            Bitmap* fileBmp = Bitmap::FromFile(firstFilePath.c_str());
            if (fileBmp && fileBmp->GetLastStatus() == Ok && fileBmp->GetWidth() > 0 && fileBmp->GetHeight() > 0) {
                if (CopyBitmapToClipboard(fileBmp)) {
                    hasDirectClipImage = true;
                }
            }
            delete fileBmp;
        }
    }

    if (!hasDirectClipImage && IsClipboardFormatAvailable(CF_BITMAP)) {
        Bitmap* bmpCopy = nullptr;
        if (OpenClipboard(hTrayWnd)) {
            HBITMAP hBmp = (HBITMAP)GetClipboardData(CF_BITMAP);
            HPALETTE hPal = IsClipboardFormatAvailable(CF_PALETTE) ? (HPALETTE)GetClipboardData(CF_PALETTE) : nullptr;
            if (hBmp) {
                Bitmap* tmp = Bitmap::FromHBITMAP(hBmp, hPal);
                if (tmp && tmp->GetLastStatus() == Ok && tmp->GetWidth() > 0 && tmp->GetHeight() > 0) {
                    int w = (int)tmp->GetWidth();
                    int h = (int)tmp->GetHeight();
                    bmpCopy = new Bitmap(w, h, PixelFormat32bppARGB);
                    Graphics g(bmpCopy);
                    g.SetCompositingMode(CompositingModeSourceCopy);
                    g.DrawImage(tmp, 0, 0, w, h);
                }
                delete tmp;
            }
            CloseClipboard();
        }
        if (bmpCopy) {
            if (CopyBitmapToClipboard(bmpCopy)) {
                hasDirectClipImage = true;
            }
            delete bmpCopy;
        }
    }

    if (!hasDirectClipImage) {
        ShowTrayToast(
            L"No image found in clipboard",
            L"Copy an image first, then use " + FormatHotkeyString(hkSearchGoogle) + L" to search on https://lens.google.com/."
        );
        return;
    }

    if (translateToTarget) {
        OpenGoogleTranslateInBrowser(translateTargetLang);
        ShowTrayToast(
            L"Directing to Google Translate",
            L"Image copied to clipboard, Press Ctrl + V once the page loaded"
        );
    } else {
        OpenGoogleLensInBrowser();
        ShowTrayToast(
            L"Directing to Google image search",
            L"Image copied to clipboard, Press Ctrl + V once the page loaded"
        );
    }
}


static HCURSOR CreateCursorFromBitmap(Bitmap& bmp, DWORD hotX, DWORD hotY) {
    HICON hIcon = nullptr;
    if (bmp.GetHICON(&hIcon) != Ok || !hIcon) return LoadCursorW(nullptr, IDC_ARROW);
    ICONINFO ii = {0};
    if (!GetIconInfo(hIcon, &ii)) {
        DestroyIcon(hIcon);
        return LoadCursorW(nullptr, IDC_ARROW);
    }
    ii.fIcon = FALSE;
    ii.xHotspot = hotX;
    ii.yHotspot = hotY;
    HCURSOR hCur = (HCURSOR)CreateIconIndirect(&ii);
    if (ii.hbmColor) DeleteObject(ii.hbmColor);
    if (ii.hbmMask) DeleteObject(ii.hbmMask);
    DestroyIcon(hIcon);
    return hCur ? hCur : LoadCursorW(nullptr, IDC_ARROW);
}

void PepperSnapDaemon::InitCustomCursors() {
    // Custom Pen Cursor (nib hotspot at (3, 28))
    {
        Bitmap bmp(32, 32, PixelFormat32bppARGB);
        Graphics g(&bmp);
        g.SetSmoothingMode(SmoothingModeAntiAlias);

        // Pen barrel angled from top-right (26, 5) down-left to nib (3, 28)
        PointF barrel[4] = {
            PointF(9.0f, 18.0f),
            PointF(23.0f, 4.0f),
            PointF(27.0f, 8.0f),
            PointF(13.0f, 22.0f)
        };
        SolidBrush barrelFill(Color(255, 239, 68, 68));
        Pen darkOutline(Color(255, 15, 23, 42), 1.6f);
        g.FillPolygon(&barrelFill, barrel, 4);
        g.DrawPolygon(&darkOutline, barrel, 4);

        // White collar + metallic pen nib cone pointing to (3, 28)
        PointF nibCone[3] = {
            PointF(3.0f, 28.0f),
            PointF(9.0f, 18.0f),
            PointF(13.0f, 22.0f)
        };
        SolidBrush nibFill(Color(255, 248, 250, 252));
        g.FillPolygon(&nibFill, nibCone, 3);
        g.DrawPolygon(&darkOutline, nibCone, 3);

        // Ink tip dot at (3, 28)
        SolidBrush tipBrush(Color(255, 239, 68, 68));
        g.FillEllipse(&tipBrush, 1.5f, 26.5f, 3.5f, 3.5f);

        hCurPen = CreateCursorFromBitmap(bmp, 3, 28);
    }

    // Custom Stabilo Boss Highlighter Cursor (chisel tip hotspot at (4, 27))
    {
        Bitmap bmp(32, 32, PixelFormat32bppARGB);
        Graphics g(&bmp);
        g.SetSmoothingMode(SmoothingModeAntiAlias);

        Pen darkOutline(Color(255, 15, 23, 42), 1.6f);

        // Broad Stabilo fluorescent yellow-orange body
        PointF body[4] = {
            PointF(8.0f, 17.0f),
            PointF(20.0f, 5.0f),
            PointF(27.0f, 12.0f),
            PointF(15.0f, 24.0f)
        };
        SolidBrush bodyFill(Color(255, 250, 204, 21)); // Classic Stabilo fluorescent yellow
        g.FillPolygon(&bodyFill, body, 4);
        g.DrawPolygon(&darkOutline, body, 4);

        // Stabilo side grip stripe
        Pen stripePen(Color(255, 254, 240, 138), 2.0f);
        g.DrawLine(&stripePen, 11.5f, 17.5f, 21.5f, 7.5f);

        // Black neck collar
        PointF collar[4] = {
            PointF(6.0f, 20.0f),
            PointF(8.0f, 17.0f),
            PointF(15.0f, 24.0f),
            PointF(12.0f, 26.0f)
        };
        SolidBrush collarFill(Color(255, 30, 41, 59));
        g.FillPolygon(&collarFill, collar, 4);

        // Angled chisel felt tip at (3, 28) .. (8, 28)
        PointF chisel[4] = {
            PointF(3.0f, 28.0f),
            PointF(6.0f, 20.0f),
            PointF(12.0f, 26.0f),
            PointF(8.0f, 29.0f)
        };
        SolidBrush chiselFill(Color(255, 163, 230, 53)); // Bright neon highlighter tip
        g.FillPolygon(&chiselFill, chisel, 4);
        g.DrawPolygon(&darkOutline, chisel, 4);

        hCurStabilo = CreateCursorFromBitmap(bmp, 4, 27);
    }
}

void PepperSnapDaemon::DestroyCustomCursors() {
    if (hCurPen) { DestroyCursor(hCurPen); hCurPen = nullptr; }
    if (hCurStabilo) { DestroyCursor(hCurStabilo); hCurStabilo = nullptr; }
}

void PepperSnapDaemon::UpdateOverlayCursor(int mx, int my) {
    if (dragMode == DragMode::ResizingFrameStrip) {
        SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
        return;
    }
    if (dragMode == DragMode::ScrollingFrameStrip) {
        SetCursor(LoadCursorW(nullptr, IDC_HAND));
        return;
    }
    if (dragMode == DragMode::DraggingHUD) {
        SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
        return;
    }
    POINT pt{ mx, my };
    if (HasOverlayMultiFrames()) {
        if (PtInRect(&overlayFrameStripToggleBtnRect, pt)) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return;
        }
        if (!overlayFrameStripCollapsed) {
            if (PtInRect(&overlayFrameStripResizeRect, pt)) {
                SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
                return;
            }
            if (PtInRect(&overlayFrameStripPanelRect, pt)) {
                if (overlayHoveredFrameIdx >= 0 ||
                    PtInRect(&overlayFrameStripScrollTrackRect, pt) ||
                    PtInRect(&overlayFrameStripScrollLeftRect, pt) ||
                    PtInRect(&overlayFrameStripScrollRightRect, pt)) {
                    SetCursor(LoadCursorW(nullptr, IDC_HAND));
                } else {
                    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
                }
                return;
            }
        }
    }
    for (const auto& b : dockButtons) {
        if (PtInRect(&b.rect, pt)) {
            if (b.id == DBTN_ACT_DRAG_HUD) {
                SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
            } else {
                SetCursor(LoadCursorW(nullptr, IDC_HAND));
            }
            return;
        }
    }

    if (hasSelection && hasCustomRatio && PtInRect(&ratioResetBtnRect, pt)) {
        SetCursor(LoadCursorW(nullptr, IDC_HAND));
        return;
    }
    if (hasSelection && (PtInRect(&dimPillRect, pt) || PtInRect(&ratioPillRect, pt) || PtInRect(&customStrokeRect, pt))) {
        SetCursor(LoadCursorW(nullptr, IDC_IBEAM));
        return;
    }
    if (isEditingText) {
        RECT tbRc = GetEditingTextBoxRect();
        if (PtInRect(&tbRc, pt)) {
            SetCursor(LoadCursorW(nullptr, IDC_IBEAM));
            return;
        }
    }

    DragMode handleHit = (dragMode >= DragMode::ResizeTL && dragMode <= DragMode::ResizeL)
        ? dragMode
        : HitTestSelectionHandles(mx, my);
    switch (handleHit) {
        case DragMode::ResizeTL:
        case DragMode::ResizeBR:
            SetCursor(LoadCursorW(nullptr, IDC_SIZENWSE));
            return;
        case DragMode::ResizeTR:
        case DragMode::ResizeBL:
            SetCursor(LoadCursorW(nullptr, IDC_SIZENESW));
            return;
        case DragMode::ResizeT:
        case DragMode::ResizeB:
            SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
            return;
        case DragMode::ResizeL:
        case DragMode::ResizeR:
            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
            return;
        default:
            break;
    }

    if (!hasSelection || dragMode == DragMode::CreatingSelection) {
        if (!hasSelection && hasCtrlHoverWindow) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
        } else {
            SetCursor(LoadCursorW(nullptr, IDC_CROSS));
        }
        return;
    }

    // Tool-specific cursors per user specification:
    // - Arrow cursor for Select mode (or when holding Ctrl to temporarily use Select mode)
    // - Pen cursor for Pen
    // - Stabilo cursor for Highlighter
    // - (+ 'plus') cursor for: Line, Square, Circle, Mosaic Square, Mosaic Circle, Arrow, Numbering Arrow
    // - Arrow cursor for Text
    switch (GetEffectiveTool()) {
        case OverlayTool::SelectMove:
            SetCursor(LoadCursorW(nullptr, IDC_ARROW));
            break;
        case OverlayTool::Pen:
            SetCursor(hCurPen ? hCurPen : LoadCursorW(nullptr, IDC_ARROW));
            break;
        case OverlayTool::Highlighter:
            SetCursor(hCurStabilo ? hCurStabilo : LoadCursorW(nullptr, IDC_ARROW));
            break;
        case OverlayTool::TextBox:
            SetCursor(LoadCursorW(nullptr, IDC_ARROW));
            break;
        case OverlayTool::Line:
        case OverlayTool::Rectangle:
        case OverlayTool::Ellipse:
        case OverlayTool::MosaicSquare:
        case OverlayTool::MosaicCircle:
        case OverlayTool::Arrow:
        case OverlayTool::NumberArrow:
        default:
            SetCursor(LoadCursorW(nullptr, IDC_CROSS));
            break;
    }
}

