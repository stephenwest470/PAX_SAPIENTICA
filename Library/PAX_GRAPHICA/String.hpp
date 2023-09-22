﻿/*##########################################################################################

    PAX SAPIENTICA Library 💀🌿🌏

    [Planning]		2023 As Project
    [Production]	2023 As Project
    [Contact Us]	wanotaitei@gmail.com			https://github.com/AsPJT/PAX_SAPIENTICA
    [License]		Distributed under the CC0 1.0.	https://creativecommons.org/publicdomain/zero/1.0/

##########################################################################################*/

#ifndef PAX_GRAPHICA_STRING_HPP
#define PAX_GRAPHICA_STRING_HPP

/*##########################################################################################

##########################################################################################*/

#include <string>

#if defined(PAXS_USING_SIV3D)
#include <Siv3D.hpp>
#elif defined(PAXS_USING_SFML)
#include <SFML/Graphics.hpp>
#elif defined(PAXS_USING_DXLIB)
#include <DxLib.h>
#endif

namespace paxg {

    struct String
    {
#if defined(PAXS_USING_SIV3D)
        s3d::String string;
        String(const s3d::String& string) : string(string) {}
        String(const std::string& string) : string(s3d::Unicode::FromUTF8((string))) {}
        operator s3d::String() const { return string; }
        // #elif defined(PAXS_USING_SFML)
        //         sf::String string;
        //         String(const sf::String& string) : string(string) {}
        //         String(const std::string& string) : string(string.c_str()) {}
        //         operator sf::String() const { return string; }
#else
        std::string string;
        String(const std::string& string) : string(string) {}
        operator std::string() const { return string; }
#endif
    };

    struct Font {
        constexpr Font() = default;
#if defined(PAXS_USING_SIV3D)
        s3d::Font font{};
        Font(const int size_, const std::string& path, const int buffer_thickness)
            :font(s3d::FontMethod::SDF, size_, s3d::Unicode::FromUTF8(path)) {
            font.setBufferThickness(buffer_thickness);
        }
        bool is_outline = false;
        s3d::TextStyle outline{};
        void setOutline(const double inner, const double outer, const paxg::Color& color) {
            is_outline = true;
            outline = s3d::TextStyle::Outline(inner, outer, color.color);
        }

        void drawBottomLeft(const std::string& str, const paxg::Vec2i pos, const paxg::Color& color) const {
            if (is_outline) {
                font(s3d::Unicode::FromUTF8(str)).draw(
                    outline,
                    s3d::Arg::bottomLeft = s3d::Vec2(pos.x(), pos.y()),
                    color.color);
            }
            else {
                font(s3d::Unicode::FromUTF8(str)).draw(
                    s3d::Arg::bottomLeft = s3d::Vec2(pos.x(), pos.y()),
                    color.color);
            }
        }
        void drawTopRight(const std::string& str, const paxg::Vec2i pos, const paxg::Color& color) const {
            if (is_outline) {
                font(s3d::Unicode::FromUTF8(str)).draw(
                    outline,
                    s3d::Arg::topRight = s3d::Vec2(pos.x(), pos.y()),
                    color.color);
            }
            else {
                font(s3d::Unicode::FromUTF8(str)).draw(
                    s3d::Arg::topRight = s3d::Vec2(pos.x(), pos.y()),
                    color.color);
            }
        }
        void draw(const std::string& str, const paxg::Vec2i pos, const paxg::Color& color) const {
            if (is_outline) {
                font(s3d::Unicode::FromUTF8(str)).draw(
                    outline,
                    s3d::Vec2(pos.x(), pos.y()),
                    color.color);
            }
            else {
                font(s3d::Unicode::FromUTF8(str)).draw(
                    s3d::Vec2(pos.x(), pos.y()),
                    color.color);
            }
        }

        int height() const {
            return font.height();
        }
        int width(const std::string& str_) {
            return font(s3d::Unicode::FromUTF8(str_)).region().w;
        }
#elif defined(PAXS_USING_DXLIB)
        int font{}; int h{ 0 };
        Font(const int size_, const std::string& path, const int buffer_thickness) {
            font = DxLib::CreateFontToHandle(NULL, size_, buffer_thickness);
            h = size_;
        }
        void setOutline(const double inner, const double outer, const paxg::Color& color) {

        }

        void drawBottomLeft(const std::string& str, const paxg::Vec2i& pos, const paxg::Color& color) const {
            DxLib::DrawFormatString(pos.x(), pos.y(), DxLib::GetColor(color.r, color.g, color.b), str.c_str());
            // DxLib::DrawStringToHandle(pos.x(), pos.y(), str.c_str(), DxLib::GetColor(color.r, color.g, color.b), font);
        }
        void drawTopRight(const std::string& str, const paxg::Vec2i& pos, const paxg::Color& color) const {
            // printfDx("%s, x%d, y%d, r%d, g%d, b%d, f%d\n", str.c_str(), pos.x(), pos.y(), color.r, color.g, color.b, font);
            int dswth = DxLib::GetDrawStringWidthToHandle(str.c_str(), int(str.size()), font);
            int dsw = DxLib::GetDrawStringWidth(str.c_str(), int(str.size()));
            // printfDx("dswth:%d, dsw:%d\n", dswth, dsw);
            DxLib::DrawFormatString(pos.x() - 300, pos.y(), DxLib::GetColor(color.r, color.g, color.b), str.c_str());

        }
        void draw(const std::string& str, const paxg::Vec2i& pos, const paxg::Color& color) const {
            DxLib::DrawFormatString(pos.x(), pos.y(), DxLib::GetColor(color.r, color.g, color.b), str.c_str());
            // DxLib::DrawStringToHandle(pos.x(), pos.y(), str.c_str(), DxLib::GetColor(color.r, color.g, color.b), font);
        }

        int height() const {
            return h;
        }
        int width(const std::string& str_) {
            return DxLib::GetDrawStringWidth(str_.c_str(), int(str_.size()));
            // return str_.size() * h;
        }
#else
        Font(const int size_, const std::string& path, const int buffer_thickness) {
        }
        void setOutline(const double inner, const double outer, const paxg::Color& color) {
        }

        void drawBottomLeft(const std::string& str, const paxg::Vec2i& pos, const paxg::Color& color) const {
        }
        void drawTopRight(const std::string& str, const paxg::Vec2i& pos, const paxg::Color& color) const {
        }
        void draw(const std::string& str, const paxg::Vec2i& pos, const paxg::Color& color) const {
        }

        int height() const {
            return 0;
        }
        int width(const std::string& str_) {
            return 0;
        }
#endif
    };

}

#endif // !PAX_GRAPHICA_STRING_HPP
