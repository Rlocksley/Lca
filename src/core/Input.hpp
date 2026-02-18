#pragma once

#include "Global.hpp"

namespace Lca
{
    namespace Input
    {
        struct Key
        {
            bool down;
            bool pressed;
            float lastPressed;
        };

        struct Button
        {
            bool down;
            bool pressed;
            float lastPressed;
        };

        struct Cursor
        {
            glm::vec2 position;
            glm::vec2 deltaPosition;
        };

        struct Scroll
        {
            float scroll;
            float deltaScroll;
        };

        inline float keyPressTime{0.2};

        // Alphabet keys
        inline Key keyA{false, false, 0.f};
        inline Key keyB{false, false, 0.f};
        inline Key keyC{false, false, 0.f};
        inline Key keyD{false, false, 0.f};
        inline Key keyE{false, false, 0.f};
        inline Key keyF{false, false, 0.f};
        inline Key keyG{false, false, 0.f};
        inline Key keyH{false, false, 0.f};
        inline Key keyI{false, false, 0.f};
        inline Key keyJ{false, false, 0.f};
        inline Key keyK{false, false, 0.f};
        inline Key keyL{false, false, 0.f};
        inline Key keyM{false, false, 0.f};
        inline Key keyN{false, false, 0.f};
        inline Key keyO{false, false, 0.f};
        inline Key keyP{false, false, 0.f};
        inline Key keyQ{false, false, 0.f};
        inline Key keyR{false, false, 0.f};
        inline Key keyS{false, false, 0.f};
        inline Key keyT{false, false, 0.f};
        inline Key keyU{false, false, 0.f};
        inline Key keyV{false, false, 0.f};
        inline Key keyW{false, false, 0.f};
        inline Key keyX{false, false, 0.f};
        inline Key keyY{false, false, 0.f};
        inline Key keyZ{false, false, 0.f};

        // Numeric keys
        inline Key key0{false, false, 0.f};
        inline Key key1{false, false, 0.f};
        inline Key key2{false, false, 0.f};
        inline Key key3{false, false, 0.f};
        inline Key key4{false, false, 0.f};
        inline Key key5{false, false, 0.f};
        inline Key key6{false, false, 0.f};
        inline Key key7{false, false, 0.f};
        inline Key key8{false, false, 0.f};
        inline Key key9{false, false, 0.f};

        // Arrow keys
        inline Key keyLeft{false, false, 0.f};
        inline Key keyRight{false, false, 0.f};
        inline Key keyUp{false, false, 0.f};
        inline Key keyDown{false, false, 0.f};

        // Special keys
        inline Key keySpace{false, false, 0.f};
        inline Key keyEsc{false, false, 0.f};
        inline Key keyShift{false, false, 0.f};
        inline Key keyEnter{false, false, 0.f};
    inline Key keyBackspace{false, false, 0.f};
        inline Key keyTab{false, false, 0.f};

        inline float buttonPressTime{0.1};

        inline Button buttonLeft{false, false, 0.f};
        inline Button buttonRight{false, false, 0.f};

        inline Cursor cursor;

        inline Scroll scroll;

        void createInput();
        void updateInput();
    }
}