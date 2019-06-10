# Amundsen
#### General

Amundsen is an XBoard compatible chess playing program, and it can participate in computer chess tournaments. Author is John Bergbom and the program is released under the GNU GPL license v. 2. From the beginning it was a college course project, but I kept working on it now and then when I had time until 2009 when active development stopped. Version 0.75 has a FICS rating of about 2240 in blitz (< 15 min), 2330 in lightning (< 5 minutes), and 2240 in standard (>= 15 min). 

A name change took place between version 0.25 and 0.3. The reason was that when the program started playing at FICS, the name jonte was taken already. 

Some matches against other engines:

amundsen v. 0.80 vs. crafty 20.14: 16-57-27

amundsen v. 0.75 vs. crafty 20.14: 15-72-13

amundsen v. 0.70 vs. crafty 20.14: 8-74-18

amundsen v. 0.65 vs. crafty 20.14: 13-81-6



amundsen v. 0.80 vs. gnuchess v. 5.07: 57-23-20

amundsen v. 0.75 vs. gnuchess v. 5.07: 59-24-17

amundsen v. 0.70 vs. gnuchess v. 5.07: 45-32-23

amundsen v. 0.65 vs. gnuchess v. 5.07: 30-52-18


#### Testsuites

Amundsen is capable of running testsuites through the "testsuite" command. For information on testsuite scores, instructions and epd files, click here.

#### Source code

[amundsen-0.80.tar.gz](https://github.com/johnbergbom/Amundsen/blob/master/old_versions/amundsen-0.80.tar.gz)

[amundsen-0.75.tar.gz](https://github.com/johnbergbom/Amundsen/blob/master/old_versions/amundsen-0.75.tar.gz)

[amundsen-0.70.tar.gz](https://github.com/johnbergbom/Amundsen/blob/master/old_versions/amundsen-0.70.tar.gz)

[amundsen-0.65.1.tar.gz](https://github.com/johnbergbom/Amundsen/blob/master/old_versions/amundsen-0.65.1.tar.gz)

[amundsen_v0.65.tar.gz](https://github.com/johnbergbom/Amundsen/blob/master/old_versions/amundsen_v0.65.tar.gz)

[amundsen_v0.60.tar.gz](https://github.com/johnbergbom/Amundsen/blob/master/old_versions/amundsen_v0.60.tar.gz)

[amundsen_v0.55.1.tar.gz](https://github.com/johnbergbom/Amundsen/blob/master/old_versions/amundsen_v0.55.1.tar.gz)

[amundsen_v0.55.tar.gz](https://github.com/johnbergbom/Amundsen/blob/master/old_versions/amundsen_v0.55.tar.gz)

[amundsen_v0.50.tar.gz](https://github.com/johnbergbom/Amundsen/blob/master/old_versions/amundsen_v0.50.tar.gz)

[amundsen_v0.45.tar.gz](https://github.com/johnbergbom/Amundsen/blob/master/old_versions/amundsen_v0.45.tar.gz)

[amundsen_v0.40.tar.gz](https://github.com/johnbergbom/Amundsen/blob/master/old_versions/amundsen_v0.40.tar.gz)

[amundsen_v0.35.tar.gz](https://github.com/johnbergbom/Amundsen/blob/master/old_versions/amundsen_v0.35.tar.gz)

[amundsen_v0.30.tar.gz](https://github.com/johnbergbom/Amundsen/blob/master/old_versions/amundsen_v0.30.tar.gz)

[jonte_v0.25.tar.gz](https://github.com/johnbergbom/Amundsen/blob/master/old_versions/jonte_v0.25.tar.gz)

[jonte_v0.20.tar.gz](https://github.com/johnbergbom/Amundsen/blob/master/old_versions/jonte_v0.20.tar.gz)

[jonte_v0.15.tar.gz](https://github.com/johnbergbom/Amundsen/blob/master/old_versions/jonte_v0.15.tar.gz)

[jonte_v0.10.tar.gz](https://github.com/johnbergbom/Amundsen/blob/master/old_versions/jonte_v0.10.tar.gz)

[jonte_v_pre0.10.tar.gz](https://github.com/johnbergbom/Amundsen/blob/master/old_versions/jonte_v_pre0.10.tar.gz)

Feel free to download the code and do whatever you want with it.

#### Opening book

Put the opening book in the same directory as the executable: [openings.bok](https://github.com/johnbergbom/Amundsen/blob/master/opening_book/openings.bok)

#### Windows binaries

###### Dann Corbit's builds:
[amundsen_0_65_1.exe](https://github.com/johnbergbom/Amundsen/blob/master/builds/amundsen_0_65_1.exe) (v. 0.65.1) 

###### Jim Ablett's builds

[amundsen080_ja.zip](https://github.com/johnbergbom/Amundsen/blob/master/builds/amundsen080_ja.zip) (win32 + x64) 

[amundsen075_ja.zip](https://github.com/johnbergbom/Amundsen/blob/master/builds/amundsen075_ja.zip) (win32 + x64) 

[amundsen070.zip](https://github.com/johnbergbom/Amundsen/blob/master/builds/amundsen070.zip) (win32 + x64) 

[amundsen_0651_x64-win32_ja.zip](https://github.com/johnbergbom/Amundsen/blob/master/builds/amundsen_0651_x64-win32_ja.zip) (win32 + x64 + universal build for older processors) 

#### Debian/Ubuntu packets

[amundsen_0.80-1_i386.deb](https://github.com/johnbergbom/Amundsen/blob/master/debian_packages/amundsen_0.80-1_i386.deb) 

[amundsen_0.75-1_i386.deb](https://github.com/johnbergbom/Amundsen/blob/master/debian_packages/amundsen_0.75-1_i386.deb) 

[amundsen_0.70-1_i386.deb](https://github.com/johnbergbom/Amundsen/blob/master/debian_packages/amundsen_0.70-1_i386.deb) 

[amundsen_0.65.1-1_i386.deb](https://github.com/johnbergbom/Amundsen/blob/master/debian_packages/amundsen_0.65.1-1_i386.deb)

#### Compilation instructions

```
Debian Linux:     Install the amundsen_0.75-1_i386.deb packet that can be
                  downloaded from bergbomconsulting.se/chess
Other Linux/UNIX: Run the following commands to create a make file
                  that's tailored to your system:
                  aclocal
                  autoconf
                  autoheader
                  automake -a
                  ./configure
                  (If you don't have all of these programs installed and you
                  don't feel like installing them, then you can always download
                  version 0.50 which has a very simple makefile, and just use
                  that one.)

                  Finally to do the actual compilation run "make".

Windows:          I've never tried it. However others have gotten it
                  to compile under Windows. To make it easier for people
                  using this platform, I have incorporated some
                  modifications made by Dann Corbit into the code. To
                  use those modifications, try compiling with the
                  _MSC_VER macro defined. You can also check Dann Corbits
                  website at:
                  ftp://cap.connx.com/pub/chess-engines/new-approach/amundsen.zip

Other platforms:  Don't know.

NOTE: amundsen works with gcc's -O3 optimization.
```

#### Usage instructions

In order to use the opening book you need to have it available in the
current directory.

#### Version history

###### Version pre-0.1 (file jonte_v_pre0.1.tar) was released in July 2003.
It plays chess using alfabeta search, and the only evaluation that is done
is looking at the material balance. It doesn't know passant moves,
50-move rule, and the 3-move rule, also it allows castling while in check.
Supports xboard interface. No timing supported. Old code for move
generation.



###### Version 0.1 (file jonte_v0.1.tar) was released in August 2003.
Passant moves are added, and castling in check is not allowed. In this version
the board representation has switched from a simple array of ints to a bitboard
representation. This version is a little buggy. After playing for a while it
starts to make illegal moves, such as leaving the king in check.



###### Version 0.15: (released in August 2003)
Move ordering added. Improved evaluation => check for doubled pawns is done.
Faster code for finding a bit position in a bitboard.



###### Version 0.2: (released in August 2003)
Faster move generation. More of the job is done in the alphabeta search
routine, to avoid duplication of work. The code gets messier this way though.
Killer moves added. In this version the engine will always promote a pawn to
a queen.



###### Version 0.25: (released in September 2003)
Iterative deepening with aspiration windows added. Time control.
Improved quiescence. Now the engine has the option of continuing a capture
line, or to accept the static score of the position.
Added checking to see if the user's move is legal.
50-move rule added.
Fixed a bug with the bitboards.



###### Version 0.3: (released in September 2003)
Better time control. Support for increment. Ability to play at FICS.
Null-move pruning added. Bug with the castling rights fixed.
Handles KRK and KQK endgames. Check extensions added. Opening book added.
Improved evaluation. FICS rating: 1590 blitz, 1680 lightning, 1800 standard



###### Version 0.35: (released in August 2004)
Fixed a bug with the opening book. Fixed a bug with the 50-move rule.
Transposition tables added. Improved move ordering, hash move, and static
exchange evaluation (SEE). Improved quiescence search. A bug fixed in the
alphabeta search. Another bug in the nullmove pruning fixed, plus a
suboptimal implementation of nullmove pruning fixed. Rating: About
the same as the previous version at FICS. In self-play however,
version 0.35 beats version 0.3 around 3 times out of four.



###### Version 0.4: (released in February 2005)
Dynamic move ordering at the root node (bug: this should have been done a
long time ago...) Fixed a transposition table bug (code for storing nodes
where no cutoff occurred was faulty). Improved re-searching of failed
aspiration searches. Improved pawn evaluation and piece placement evaluation.
Pawn hashtable added. Trades pieces if ahead, and trades pawns if materially
behind. Addition made so that moves which are likely to be bad are skipped at
deeper levels of the search. Somewhat better endgame handling.
This version beats version 0.35 about 60% of the time.
FICS rating: 1760 blitz, 1850 lightning, 1900 standard
I believe this rating boost comes mainly from better hardware.



###### Version 0.45: (released in June 2005)
Hashtables are now divided into a smaller refutation table and a
transposition table. 3-fold repetition finally added, which means
that Amundsen now actually implements all chess rules. Somewhat better
evaluation function after input from Dann Corbit. Support for the
xboard "ping" command added. Better time handling. This version beats
version 0.4 around 60-70% of the time.
FICS rating: 1830 blitz, 2050 lightning, 1950 standard



###### Version 0.50: (released in September 2005)
Support for the xboard "setboard", "sd" and "st" commands. Fixed a bug
that sometimes caused a segmentation fault. Support for running testsuites
through the "testsuite" command. Several hashtable fixes: Saved the best
move in the hashtable also when no cutoff occurred. Two bugs fixed when
closing a transposition entry when the transposition table returns a miss
and the refutation table returns a hit. Minor fix of bounds for status flag
when saving a position in the transposition table when no cutoff occurred.
Improved mate handling in the transposition table. Better heuristics for
skipping searching moves that are likely to be bad. Fixed a GIGANTIC bug in
the alphabeta search: The alpha bound was previously not adjusted during
the search of root nodes. This version beats version 0.45 around 80% of
the time.
FICS rating: 1950 blitz, 2140 lightning, 2080 standard



###### Version 0.55: (released in November 2006)
Bugfix: The repetition counter previously wasn't reset when a new game starts.
Fixed faulty detection of passed pawns. Improved quiescence search using a
very fast swap()-function for determining what moves are worth trying, and
then doing futility pruning on the moves. Turned the maxmin search routine
into a negamax search. Slightly better move ordering at the root node.
Slightly speeded up generate_moves()-function.

Eliminating redundant calls to the generate_moves()-function:
1.) By using a fast opp_in_check()-function it's possible to return before
the move generation stage in case of illegal positions.
2.) In full-width search: Capture moves are generated using the
swap()-function, and if a good capture causes a cutoff then return before
the generate_moves()-function is called.
3.) In quiescence search: The moves are generated using only the
swap()-function and not the much slower generate_moves()-function.

Improved null-move handling:
1.) Now the quiescence search is called at the end of the null move search
instead of doing just static evaluation. The average search depth went down
with about 0.5 ply when doing this but the gained accuracy still made it
pay off big time.
2.) If the null-move search returns that the player making the null-move
is "mated in N", then extend the search.
3.) Trimmed condition for when to stop doing nullmove searches. Earlier
nullmove search wasn't done when either player had less than two pieces, but
now nullmove search doesn't stop until one of the players has only the king
left.
4.) Implemented adaptive null-move search (R = 3 in the upper part of the
search tree except for in the late endgame).

Better move ordering in the full width search by:
1.) Using the swap()-function for static exchange evaluation (SEE).
2.) Peek into the hashtable to see if any of the moves will raise
alpha right away.
3.) History moves.
4.) Promotion moves.

This version beats version 0.50 almost 90% of the time.
FICS rating: 1990 blitz, 2160 lightning, 2110 standard


###### Version 0.55.1: (released in December 2006)
Bugfix: Made sure that the engine doesn't bail out with floating point
exception if the xboard "level" command isn't given. Credits to
Robert Ancell for pointing out this bug. No new features added.
FICS rating: Same as the previous version.


###### Version 0.60 (released in August 2007)
Hashtable bugs fixed:
1.) Made sure that extension/reduction of depth is done before probing
the hashtable and not after.
2.) Corrected suboptimal implementation of mate score handling in the
hashtable.
3.) Don't store draw by repetition values in the hashtable since it's
a path dependent value. Rather just cutoff without storing anything
if a repetition is detected.
4.) prepare_research() is removed => This was a function that cleared
the hashtable before doing a research upon fail high/low. It should
never have been created in the first place. (removing this function
gave a clear improvement)
5.) Better interaction between the transposition table probing and
the hashtable probing.
6.) Don't store anything in the transposition table when depth = 0.
7.) Fixed suboptimal handling of best-move in the hashtable.
8.) A few other small improvements.

Bugfixes:
1.) Corrected faulty <= into a >= to make sure that if the opponent
made a move that causes a 3-fold repetition, then we'll only claim
a draw if we are worse off (and not better off!)
2.) Fix where some mate scores were previously not recognized.
3.) Check faulty castling rights when parsing fen/epd-strings.

Features added:
1.) Very simple internal node recognizer that recognizes draws due
to insufficient material.
2.) Added the "perft" command.
3.) Added futility pruning, extended futility pruning and limited
razoring. This means Amundsen now implements full AEL pruning.
4.) Made sure that no "stand pat" cutoffs are allowed in quiescence
search if the king is in check (this greatly boosted the playing strength).
5.) Quiescence can return also mate scores.
6.) Added pawnpush extension.
7.) More cpu cache efficient implementation by replacing the old makemove
function that always copies the old board, with a makemove/unmakemove scheme.
=> fewer cache misses, higher number of nodes per second. This new scheme
also made it possible to efficiently implement a few other improvements,
for example adding a board-array to the board struct, which makes it
possible to quickly answer questions like "What piece is located at
square A4?".
8.) Better time management. This noticably boosted the playing strength
as well as raised the average search depth with about 0.5 ply.
9.) Late move reduction added, which greatly improved the playing
strength (and increased the average search depth with almost 0.7 ply).
10.) Extension if the late move reduction failed.
11.) More accurate repetition detection.
12.) Improved conditions for when to do nullmove pruning.

Other changes:
1.) Removed peeking into the hashtable for move ordering.

Now for the first time I can say that the searching actually works quite
well, and the hashtable doesn't seem to contain any serious bugs anymore.

This version beats the old version about 75% of the time.
FICS rating: 2070 blitz, 2260 lightning, 2170 standard


###### Version 0.65 (released in January 2008)
Principal Variation Search added. Greatly improved evaluation function
after inspiration from Crafty (this greatly boosted the playing strength).
Much better increment time handling after input from Dann Corbit.

This version beats version 0.60 almost 90% of the time.
FICS rating: 2080 blitz, 2280 lightning, 2210 standard


###### Version 0.65.1: (released in February 2008)
Bugfixes:
Removed an array index overflow bug. Credits to Dann Corbit
for pointing it out.

Made sure that flagging draw due to insufficient material as well as
draw by repetition is done according to FIDE rules. Credits to Olivier
Deville for pointing these bugs out.

No new features added.

FICS rating: Same as the previous version.


###### Version 0.70 (released in August 2008)
Pondering finally added. The hashtable is no longer used for
detecting 3 position repetition, but rather an array based solution
is used. This simplified the hashtable implementation and also made
it easier to implement pondering properly. Much improved time handling,
especially for increment time controls. Reinefeld's pvs improvement:
re-search only if depth > 2, plus tighter re-search bound. Very simple
lazy evaluation added. Somewhat faster evaluation code. Better move
ordering: promotion moves are checked before killer moves + made sure
killer move slots are never polluted with good captures or promotions.
This made the cutoff rate go up almost 2%. Bug fixed in bad bishop
detection. Bugfix: parsing the xboard "level" command correctly also
if time isn't given in whole minutes.

This version beats version 0.65.1 between 65-80% of the time (depending
on the time controls used).
FICS rating: 2220 blitz, 2320 lightning, 2210 standard


###### Version 0.75 (released in December 2008)
King safety added using simple tropism count after inspiration from Crafty.
Better draw by repetition detection by not using the hashtable in PV nodes
for anything else than extracting best moves (no cutoffs nor bounds
adjustments based on hashvalue when in PV). Support for KRPKR endgames.
Mobility evaluation after inspiration from Rebel/Schr�der.

This version beats version 0.70 around 60-65% of the time.
FICS rating: 2240 blitz, 2330 lightning, 2240 standard


###### Version 0.80 (released in April 2009)
Made sure that killers are indexed by ply instead of by depth, in
order to better handle reductions and extensions. The cutoff-rate
increased slightly due to this.

Simple cutoff statistics in the search that is used for skipping
quiescence search if the move in question seems very unlikely to
contain any dangerous threats.

Correct calculation of effective branching factor. Internal iterative
deepening added. More efficient use of history table for move ordering
in the upper parts of the tree. Slightly improved setting of the R
parameter for the null move search. Increased search time when getting
out of opening book.

Improved LMR search:
1.) Handling also the case when depth = 2 (by then calling quiescence
    search).
2.) In some cases frontier nodes are skipped altogether.
3.) LMR also for the root node.
4.) Slightly adjusted condition for when to do LMR when ply = 3.

Endgame handling:
1.) Improved code for pawn race handling
2.) Support for the KBNK and KPK endgames.
3.) Bugfix for KRK and KQK endgames: this code previously worked only
    for black.
4.) Improved code for forcing the lone king to the side for KRK,
    KQK and unknown endgames.

This version beats version 0.75 around 60-65% of the time.


#### Links

Bruce Moreland's chess programming tips (very good!)
Advanced topics concerning chess programming
Tricky positions to test on a chess engine
Alarm chess engine
PGN Specification 
Assorted papers on computer chess. 
Winboard forum. (Contains a very good forum on chess programming)
Talkchess. (Contains a very good forum on chess programming)
