# KlaraDestroyer
KlaraDestroyer is a chess engine built from scratch written in modern C++ emphasizing efficiency. It uses the minmax algorithm to search for the best move based on material evaluation.

See it play in action or challenge it on [lichess](https://lichess.org/@/KlaraDestroyer-aB)!

Its ELO rating is around 1800, depending on game length. The program is not optimized for long games; finding relatively good moves very fast, but doesn't really improve that much, even if given more time. It excels in bullet.

On modern CPU, it can reach a depth of 8-10 plies on average.

It can ramp up CPU usage to a 100% but uses only around a few MB of RAM.

## Code
The code is a bit of a mess, as the project started small and grew bigger without really focusing on good software engineering practices. Now it still holds some relicts and could use some refactoring.

## Features
### Iterative deepening
The chess engine progressively deepens the search for the best move, prioritizing previously well evaluated moves.
### Alpha-beta pruning
The algorithm supports alpha-beta pruning to prune game paths that are strictly worse than already found.
### Multi-threaded execution
The chess engine uses one thread for each possible move from the position it is playing from. Each position is evaluated independently, but pruning occurs in-between threads
### Time management
The chess engine employs custom time management, for deciding when to play fast and when to use more time. After finishing searching in one depth, it decides if to try searching deeper based on the improvement reached so far and remaining time estimating the time for next iteration
### Position evaluation
The chess engine uses the PESTO evaluation method to determine how good each position is for each chess piece in different phases of the game. It uses a precomputed table based on https://www.chessprogramming.org/PeSTO%27s_Evaluation_Function
### Force draw
The program tries to force draw by repeating the same move, if it decides it is losing against its opponent

## TODO
Planning to maybe support in the future:
- Optimizing out already searched positions (no hash table)
- Think on opponent's time (ponder)
- En passant
- Improve code readability

## Name origin 
The name is based on the original motivation for making this chess engine: to beat my friend
