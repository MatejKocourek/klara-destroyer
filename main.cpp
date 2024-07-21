#ifndef MS_STDLIB_BUGS // Allow overriding the autodetection.
/* The Microsoft C and C++ runtime libraries that ship with Visual Studio, as
 * of 2017, have a bug that neither stdio, iostreams or wide iostreams can
 * handle Unicode input or output.  Windows needs some non-standard magic to
 * work around that.  This includes programs compiled with MinGW and Clang
 * for the win32 and win64 targets.
 */
#  if ( _MSC_VER || __MINGW32__ || __MSVCRT__ )
 /* This code is being compiled either on MS Visual C++, or MinGW, or
  * clang++ in compatibility mode for either, or is being linked to the
  * msvcrt.dll runtime.
  */
#    define MS_STDLIB_BUGS 1
#  else
#    define MS_STDLIB_BUGS 0
#  endif
#endif

#if MS_STDLIB_BUGS
#  include <io.h>
#  include <fcntl.h>
#endif

#include <fstream>
#include <iostream>
#include <queue>
#include <sstream>
#include <iomanip>



#include "KlaraDestroyer.h"

void init_locale(void)
// Does magic so that wcout can work.
{
#if MS_STDLIB_BUGS
    // Windows needs a little non-standard magic.
    constexpr char cp_utf16le[] = ".1200"; // UTF-16 little-endian locale.
    setlocale(LC_ALL, cp_utf16le);
    _setmode(_fileno(stdout), _O_WTEXT);

    /* Repeat for _fileno(stdin), if needed. */
#else
    // The correct locale name may vary by OS, e.g., "en_US.utf8".
    constexpr char locale_name[] = "";
    setlocale(LC_ALL, locale_name);
    std::locale::global(std::locale(locale_name));
    std::wcin.imbue(std::locale())
        std::wcout.imbue(std::locale());

#endif
}


auto benchmark(size_t depth)
{
    using namespace KlaraDestroyer;
    std::stringstream ss;
    ss << "uci\nsetoption name Verbosity value 1\nposition startpos\ngo infinite depth " << depth << "\nquit\n";
    std::cerr << ss.str() << std::endl;
    return uci(ss, std::cout);
}

int main(int argc, char** argv) {
    using namespace KlaraDestroyer;
    std::ios_base::sync_with_stdio(false);
    //out.setf(std::ios::fixed, std::ios::floatfield);
    //out.setf(std::ios::showpoint);
    std::cerr.setf(std::ios::fixed, std::ios::floatfield);
    std::cerr.setf(std::ios::showpoint);

    //std::cerr.sync_with_stdio(false);
    if (argc > 1)
    {
        //out.sync_with_stdio(false);
        //out.precision(2);
//#if out == std::wcout
//        init_locale();
//#endif

        std::string_view argument(argv[1]);
        if (argument == "bench")
        {
            shuffle = false;
            if (argc < 3)
                return 1;
            int n = std::atoi(argv[2]);
            benchmark(n);
        }
#ifdef _DEBUG
        else if (argument == "test")
        {
            shuffle = false;
            if (argc < 3)
                return 1;
            int n = std::atoi(argv[2]);

            switch (n)
            {
            case(1):
            {
                GameState promotion;
                promotion.pieceAt('h', '5') = Piece::PawnBlack;
                promotion.pieceAt('d', '5') = Piece::PawnBlack;
                promotion.pieceAt('f', '5') = Piece::KingBlack;
                promotion.pieceAt('g', '1') = Piece::BishopBlack;
                promotion.pieceAt('e', '2') = Piece::KingWhite;
                promotion.pieceAt('b', '5') = Piece::PawnWhite;
                promotion.pieceAt('a', '6') = Piece::PawnWhite;
                promotion.playerOnMove = PlayerSide::WHITE;

                promotion.print();

                benchmark(8, promotion);
            } break;
            case (2):
            {
                GameState testMatu;
                testMatu.pieceAt('h', '8') = Piece::KingBlack;
                testMatu.pieceAt('a', '1') = Piece::KingWhite;
                testMatu.pieceAt('g', '1') = Piece::RookWhite;
                testMatu.pieceAt('a', '7') = Piece::RookWhite;
                testMatu.pieceAt('b', '1') = Piece::QueenWhite;
                testMatu.pieceAt('c', '7') = Piece::PawnWhite;
                testMatu.playerOnMove = PlayerSide::WHITE;
                testMatu.print();

                benchmark(8, testMatu);
            } break;
            case (3):
            {
                GameState testMatu;
                testMatu.pieceAt('h', '8') = Piece::KingBlack;
                testMatu.pieceAt('h', '7') = Piece::PawnWhite;
                testMatu.pieceAt('g', '6') = Piece::PawnWhite;
                testMatu.pieceAt('h', '6') = Piece::KingWhite;
                testMatu.pieceAt('h', '5') = Piece::PawnWhite;
                testMatu.pieceAt('g', '5') = Piece::PawnWhite;

                //testMatu.deleteAndOverwritePiece('h', '4', &kingWhite);
                //testMatu.deleteAndOverwritePiece('h', '3', &pawnWhite);
                //testMatu.deleteAndOverwritePiece('g', '3', &pawnWhite);
                //testMatu.deleteAndOverwritePiece('g', '4', &pawnWhite);

                testMatu.playerOnMove = PlayerSide::WHITE;
                testMatu.print();

                benchmark(8, testMatu);
            } break;
            case (4):
            {
                GameState test = GameState::startingPosition();
                //constexpr auto tmp = test.piecesCount();

                test.pieceAt('b', '1') = Piece::Nothing;
                test.pieceAt('c', '1') = Piece::Nothing;
                test.pieceAt('d', '1') = Piece::Nothing;
                test.pieceAt('f', '1') = Piece::Nothing;
                test.pieceAt('g', '1') = Piece::Nothing;

                test.playerOnMove = PlayerSide::WHITE;
                test.print();

                benchmark(8, test);
            } break;
            case (5):
            {
                constexpr auto test = GameState::startingPosition();
                auto balance = test.balance();//SEGFAULT!
                constexpr auto counter = test.piecesCountA();
                constexpr auto min = test.countPiecesMin();
            } break;

            default:
                break;
            }
        }
#endif
        else
        {
            std::cerr << "Unknown argument" << std::endl;
            return 1;
        }
    }
    else
    {
        shuffle = true;
        return uci(std::cin,std::cout);
    }


    return 0;
}
