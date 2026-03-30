import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public class Chess {
    class AttackRelation {
        boolean isWhite;
        char fromPiece; // piece attacking
        char toPiece;  // piece attacked
        int fromGridInNum; // attacking piece pisition;  0 is a8 , 7 is h8; 63 is h1
        int toGridInNum; // attacked piece pisition
    }

    class Coord {
        /* data  coord is from 1 - 8 for both x and y */
        int x;
        int y;
    }

    ;

    // get all possible moves for designated side
    class Move // this struct record all
    {
        boolean isWhite; // side
        char type;
        // boolean fromPositionIsAttacked;
        // boolean toPositionIsAttacked;
        // char toPostionAttacks; // the piece type being attacked in to_grid
        int fromGridNum;// current position
        int toGridNum;
        int score;

        @Override
        public String toString() {
            return "Move{" +
                    "isWhite=" + isWhite +
                    ", type=" + type +
                    ", fromGridNum=" + fromGridNum +
                    ", toGridNum=" + toGridNum +
                    ", score=" + score +
                    '}';
        }
    }

    String board[] = { // all position is from the white view, with x from a-i, y from 1-8
            "rl", "nl", "bl", "q", "k", "br", "nr", "rr", // black using small letters
            "p1", "p2", "p3", "p4", "p5", "p6", "p7", "p8",
            "empty", "empty", "empty", "empty", "empty", "empty", "empty", "empty",
            "empty", "empty", "empty", "empty", "empty", "empty", "empty", "empty",
            "empty", "empty", "empty", "empty", "empty", "empty", "empty", "empty",
            "empty", "empty", "empty", "empty", "empty", "empty", "empty", "empty",
            "P1", "P2", "P3", "P4", "P5", "P6", "P7", "P8", //white pieces use capital letters
            "RL", "NL", "BL", "Q", "K", "BR", "NR", "RR"};
    boolean isWhiteCheckmated = false;
    boolean isBlackCheckmated = false;
    boolean controlledByUser = true; // true means the user control the white side
    List<Move> moves = new ArrayList<>();
    List<AttackRelation> attackRelations = new ArrayList<>();
    Set<Integer> whiteAttackingSpace;
    Set<Integer> blackAttackSpace;

    // recording castling (车王移位)
    boolean KQ = true; // if white hasn't use castling;
    boolean kq = true; // if black hasn't use castling;

    Coord num2Coord(int order) {// from 0-63, 0 is 1,8, 63 is 8,1
        char xChar;
        int x = order % 8 + 1;
        int y = 8 - ((int) order / 8);
        Coord coord = new Coord();
        coord.x = x;
        coord.y = y;
        return coord;
    }

    int coord2Num(Coord coord) {
        return coord.x * (9 - coord.y) - 1;
    }

    int num2CoordX(int order) {// from 0-63, 0 is 1,8, 63 is 8,1
        char xChar;
        int x = order % 8 + 1;
        return x;
    }

    int num2CoordY(int order) {// from 0-63, 0 is 1,8, 63 is 8,1
        int y = 8 - ((int) order / 8);
        return y;
    }

    int grid2Num(int x, int y) {
        return (8-y)*8+x-1;
    }


    boolean isOccupiedByWhite(int gridX, int gridY) {
        boolean isOccupied = false;
//        int temp = grid2Num(gridX, gridY);
//        System.out.println(gridX+", "+ gridY +" "+temp);
        char pieceType = board[grid2Num(gridX, gridY)].charAt(0);

        if (pieceType=='R' || pieceType=='N' || pieceType=='B'
                || pieceType=='Q' || pieceType=='K' || pieceType=='P')// if the piece is white and the grid is occupied by white piece
        {
            isOccupied = true;
        }
        return isOccupied;
    }

    boolean isOccupiedByBlack(int gridX, int gridY) {
        boolean isOccupied = false;
//        int temp = grid2Num(gridX, gridY);
//        System.out.println(gridX+", "+ gridY +" "+temp);
        char pieceType = board[grid2Num(gridX, gridY)].charAt(0);
        if (pieceType=='r' || pieceType=='n' || pieceType=='b'
                || pieceType=='q' || pieceType=='k' || pieceType=='p')// if the piece is white and the grid is occupied by white piece
        {
            isOccupied = true;
        }
        return isOccupied;
    }

    boolean isOccupiedByOwnSidePiece(int gridX, int gridY, boolean isWhite) {
        if (isWhite) {
            return isOccupiedByWhite(gridX, gridY);
        } else if (!isWhite) {
            return isOccupiedByBlack(gridX, gridY);
        }
        return false;
    }

    boolean isOccupiedByOpponentPiece(int gridX, int gridY, boolean isWhite) {
        if (isWhite) {
            return isOccupiedByBlack(gridX, gridY);
        } else if (!isWhite) {
            return isOccupiedByWhite(gridX, gridY);
        }
        return false;
    }

    List<Integer> getRookPossibleMoves(int gridX, int gridY, boolean isWhite, char rOrQ) { // NOTE: gridX and gridY from 1 to 8 not 0!!!
        List<Integer> possibleGrids = new ArrayList<>();

        // check the 7 horizontal grids
        // check grids on the left
        for (int i = gridX - 1; i > 0; i--) {
            char tempPiece = board[grid2Num(i, gridY)].charAt(0);
            if (tempPiece == 'e') {
                possibleGrids.add(grid2Num(i, gridY));
            } else if (isOccupiedByOwnSidePiece(i, gridY, isWhite)) {
                break;
            } else {// if it is not empty and not occupied by pieces from its side, it must be occupied by oppenent's side
                possibleGrids.add(grid2Num(i, gridY));
                AttackRelation tempRelation = new AttackRelation();
                tempRelation.isWhite = isWhite;
                tempRelation.fromGridInNum = grid2Num(gridX, gridY);
                tempRelation.fromPiece = rOrQ;
                tempRelation.toGridInNum = grid2Num(i, gridY);
                tempRelation.toPiece = tempPiece;
                attackRelations.add(tempRelation);
                break;
            }
        }
        // check grids on the right
        for (int i = gridX + 1; i < 9; i++) {
            char tempPiece = board[grid2Num(i, gridY)].charAt(0);
            if (tempPiece == 'e') {
                possibleGrids.add(grid2Num(i, gridY));
            } else if (isOccupiedByOwnSidePiece(i, gridY, isWhite)) {
                break;
            } else {// if it is not empty and not occupied by pieces from its side, it must be occupied by oppenent's side
                possibleGrids.add(grid2Num(i, gridY));
                AttackRelation tempRelation = new AttackRelation();
                tempRelation.isWhite = isWhite;
                tempRelation.fromGridInNum = grid2Num(gridX, gridY);
                tempRelation.fromPiece = rOrQ;
                tempRelation.toGridInNum = grid2Num(i, gridY);
                tempRelation.toPiece = tempPiece;
                attackRelations.add(tempRelation);
                break;
            }
        }
        //check vertial grids
        // check grids above
        for (int i = gridY + 1; i < 9; i++) {
            char tempPiece = board[grid2Num(gridX, i)].charAt(0);
            if (tempPiece == 'e') {
                possibleGrids.add(grid2Num(gridX, i));
            } else if (isOccupiedByOwnSidePiece(gridX, i, isWhite)) {
                break;
            } else {// if it is not empty and not occupied by pieces from its side, it must be occupied by oppenent's side
                possibleGrids.add(grid2Num(gridX, i));
                AttackRelation tempRelation = new AttackRelation();
                tempRelation.isWhite = isWhite;
                tempRelation.fromGridInNum = grid2Num(gridX, gridY);
                tempRelation.fromPiece = rOrQ;
                tempRelation.toGridInNum = grid2Num(gridX, i);
                tempRelation.toPiece = tempPiece;
                attackRelations.add(tempRelation);
                break;
            }
        }
        // check grids below
        for (int i = gridY - 1; i > 0; i--) {
            char tempPiece = board[grid2Num(gridX, i)].charAt(0);
            if (tempPiece == 'e') {
                possibleGrids.add(grid2Num(gridX, i));
            } else if (isOccupiedByOwnSidePiece(gridX, i, isWhite)) {
                break;
            } else {// if it is not empty and not occupied by pieces from its side, it must be occupied by oppenent's side
                possibleGrids.add(grid2Num(gridX, i));
                AttackRelation tempRelation = new AttackRelation();
                tempRelation.isWhite = isWhite;
                tempRelation.fromGridInNum = grid2Num(gridX, gridY);
                tempRelation.fromPiece = rOrQ;
                tempRelation.toGridInNum = grid2Num(gridX, i);
                tempRelation.toPiece = tempPiece;
                attackRelations.add(tempRelation);
                break;
            }
        }
        return possibleGrids;
    }

    // for knights there are 8 possible moves needed to check if there is own side's piece occupied or out of range
// unlike chinese horses, it doesnt matter if there are pieces in between
    List<Integer> getKnightPossibleMoves(int gridX, int gridY, boolean isWhite) { // cause queen can move like bishop as well
        List<Integer> possibleGrids = new ArrayList<>();
        if (gridX - 1 > 0 && gridY - 2 > 0 && !isOccupiedByOwnSidePiece(gridX - 1, gridY - 2, isWhite)) {
            possibleGrids.add(grid2Num(gridX - 1, gridY - 2));
            if (isOccupiedByOpponentPiece(gridX - 1, gridY - 2, isWhite)) {
                AttackRelation tempRelation = new AttackRelation();
                tempRelation.isWhite = isWhite;
                tempRelation.fromGridInNum = grid2Num(gridX, gridY);
                tempRelation.fromPiece = 'n';
                tempRelation.toGridInNum = grid2Num(gridX - 1, gridY - 2);
                tempRelation.toPiece = board[grid2Num(gridX - 1, gridY - 2)].charAt(0);
                attackRelations.add(tempRelation);
            }
        }
        if (gridX + 1 < 9 && gridY - 2 > 0 && !isOccupiedByOwnSidePiece(gridX + 1, gridY - 2, isWhite)) {
            possibleGrids.add(grid2Num(gridX + 1, gridY - 2));
            if (isOccupiedByOpponentPiece(gridX + 1, gridY - 2, isWhite)) {
                AttackRelation tempRelation = new AttackRelation();
                tempRelation.isWhite = isWhite;
                tempRelation.fromGridInNum = grid2Num(gridX, gridY);
                tempRelation.fromPiece = 'n';
                tempRelation.toGridInNum = grid2Num(gridX + 1, gridY - 2);
                tempRelation.toPiece = board[grid2Num(gridX + 1, gridY - 2)].charAt(0);
                attackRelations.add(tempRelation);
            }
        }
        if (gridX + 2 < 9 && gridY - 1 > 0 && !isOccupiedByOwnSidePiece(gridX + 2, gridY - 1, isWhite)) {
            possibleGrids.add(grid2Num(gridX + 2, gridY - 1));
            if (isOccupiedByOpponentPiece(gridX + 2, gridY - 1, isWhite)) {
                AttackRelation tempRelation = new AttackRelation();
                tempRelation.isWhite = isWhite;
                tempRelation.fromGridInNum = grid2Num(gridX, gridY);
                tempRelation.fromPiece = 'n';
                tempRelation.toGridInNum = grid2Num(gridX + 2, gridY - 1);
                tempRelation.toPiece = board[grid2Num(gridX + 2, gridY - 1)].charAt(0);
                attackRelations.add(tempRelation);
            }
        }
        if (gridX + 2 < 9 && gridY + 1 < 9 && !isOccupiedByOwnSidePiece(gridX + 2, gridY + 1, isWhite)) {
            possibleGrids.add(grid2Num(gridX + 2, gridY + 1));
            if (isOccupiedByOpponentPiece(gridX + 2, gridY + 1, isWhite)) {
                AttackRelation tempRelation = new AttackRelation();
                tempRelation.isWhite = isWhite;
                tempRelation.fromGridInNum = grid2Num(gridX, gridY);
                tempRelation.fromPiece = 'n';
                tempRelation.toGridInNum = grid2Num(gridX + 2, gridY + 1);
                tempRelation.toPiece = board[grid2Num(gridX + 2, gridY + 1)].charAt(0);
                attackRelations.add(tempRelation);
            }
        }
        if (gridX + 1 < 9 && gridY + 2 < 9 && !isOccupiedByOwnSidePiece(gridX + 1, gridY + 2, isWhite)) {
            possibleGrids.add(grid2Num(gridX + 1, gridY + 2));
            if (isOccupiedByOpponentPiece(gridX + 2, gridY + 1, isWhite)) {
                AttackRelation tempRelation = new AttackRelation();
                tempRelation.isWhite = isWhite;
                tempRelation.fromGridInNum = grid2Num(gridX, gridY);
                tempRelation.fromPiece = 'n';
                tempRelation.toGridInNum = grid2Num(gridX + 2, gridY + 1);
                tempRelation.toPiece = board[grid2Num(gridX + 2, gridY + 1)].charAt(0);
                attackRelations.add(tempRelation);
            }
        }
        if (gridX - 1 > 0 && gridY + 2 < 9 && !isOccupiedByOwnSidePiece(gridX - 1, gridY + 2, isWhite)) {
            possibleGrids.add(grid2Num(gridX - 1, gridY + 2));
            if (isOccupiedByOpponentPiece(gridX - 1, gridY + 2, isWhite)) {
                AttackRelation tempRelation = new AttackRelation();
                tempRelation.isWhite = isWhite;
                tempRelation.fromGridInNum = grid2Num(gridX, gridY);
                tempRelation.fromPiece = 'n';
                tempRelation.toGridInNum = grid2Num(gridX - 1, gridY + 2);
                tempRelation.toPiece = board[grid2Num(gridX - 1, gridY + 2)].charAt(0);
                attackRelations.add(tempRelation);
            }
        }
        if (gridX - 2 > 0 && gridY + 1 < 9 && !isOccupiedByOwnSidePiece(gridX - 2, gridY + 1, isWhite)) {
            possibleGrids.add(grid2Num(gridX - 2, gridY + 1));
            if (isOccupiedByOpponentPiece(gridX - 2, gridY + 1, isWhite)) {
                AttackRelation tempRelation = new AttackRelation();
                tempRelation.isWhite = isWhite;
                tempRelation.fromGridInNum = grid2Num(gridX, gridY);
                tempRelation.fromPiece = 'n';
                tempRelation.toGridInNum = grid2Num(gridX - 2, gridY + 1);
                tempRelation.toPiece = board[grid2Num(gridX - 2, gridY + 1)].charAt(0);
                attackRelations.add(tempRelation);
            }
        }
        if (gridX - 2 > 0 && gridY - 1 > 0 && !isOccupiedByOwnSidePiece(gridX - 2, gridY - 1, isWhite)) {
            possibleGrids.add(grid2Num(gridX - 2, gridY - 1));
            if (isOccupiedByOpponentPiece(gridX - 2, gridY - 1, isWhite)) {
                AttackRelation tempRelation = new AttackRelation();
                tempRelation.isWhite = isWhite;
                tempRelation.fromGridInNum = grid2Num(gridX, gridY);
                tempRelation.fromPiece = 'n';
                tempRelation.toGridInNum = grid2Num(gridX - 2, gridY - 1);
                tempRelation.toPiece = board[grid2Num(gridX - 2, gridY - 1)].charAt(0);
                attackRelations.add(tempRelation);
            }
        }
        return possibleGrids;
    }

    List<Integer> getBishopPossibleMoves(int gridX, int gridY, boolean isWhite, char bOrQ) { // cause queen also can move like bishop, so the fourth input is b or q
        List<Integer> possibleGrids = new ArrayList<>();
        // for the left down side
        for (int i = 1; i <= min(gridX - 1, gridY - 1); i++) {
            if (board[grid2Num(gridX - i, gridY - i)].equals("empty")) {
                possibleGrids.add(grid2Num(gridX - i, gridY - i));
            } else if (isOccupiedByOpponentPiece(gridX - i, gridY - i, isWhite)) {
                possibleGrids.add(grid2Num(gridX - i, gridY - i));
                AttackRelation tempRelation = new AttackRelation();
                tempRelation.isWhite = isWhite;
                tempRelation.fromGridInNum = grid2Num(gridX, gridY);
                tempRelation.fromPiece = bOrQ;
                tempRelation.toGridInNum = grid2Num(gridX - i, gridY - i);
                tempRelation.toPiece = board[grid2Num(gridX - i, gridY - i)].charAt(0);
                attackRelations.add(tempRelation);
                break;// meaning there is no need to search further
            } else if (isOccupiedByOwnSidePiece(gridX - i, gridY - i, isWhite)) {
                break;
            }
        }
// for the right down side
        for (int i = 1; i <= min(8 - gridX, gridY - 1); i++) {
            if (board[grid2Num(gridX + i, gridY - i)].equals("empty")) {
                possibleGrids.add(grid2Num(gridX + i, gridY - i));
            } else if (isOccupiedByOpponentPiece(gridX + i, gridY - i, isWhite)) {
                possibleGrids.add(grid2Num(gridX + i, gridY - i));
                AttackRelation tempRelation = new AttackRelation();
                tempRelation.isWhite = isWhite;
                tempRelation.fromGridInNum = grid2Num(gridX, gridY);
                tempRelation.fromPiece = bOrQ;
                tempRelation.toGridInNum = grid2Num(gridX + i, gridY - i);
                tempRelation.toPiece = board[grid2Num(gridX + i, gridY - i)].charAt(0);
                attackRelations.add(tempRelation);
                break;// meaning there is no need to search further
            } else if (isOccupiedByOwnSidePiece(gridX + i, gridY - i, isWhite)) {
                ;
                break;
            }
        }
// for the right up side
        for (int i = 1; i <= min(8 - gridX, 8 - gridY); i++) {
            if (board[grid2Num(gridX + i, gridY + i)].equals("empty")) {
                possibleGrids.add(grid2Num(gridX + i, gridY + i));
            } else if (isOccupiedByOpponentPiece(gridX + i, gridY + i, isWhite)) {
                possibleGrids.add(grid2Num(gridX + i, gridY + i));
                AttackRelation tempRelation = new AttackRelation();
                tempRelation.isWhite = isWhite;
                tempRelation.fromGridInNum = grid2Num(gridX, gridY);
                tempRelation.fromPiece = bOrQ;
                tempRelation.toGridInNum = grid2Num(gridX + i, gridY + i);
                tempRelation.toPiece = board[grid2Num(gridX + i, gridY + i)].charAt(0);
                attackRelations.add(tempRelation);
                break;// meaning there is no need to search further
            } else if (isOccupiedByOwnSidePiece(gridX + i, gridY + i, isWhite)) {
                ;
                break;
            }
        }
// for the left up side
        for (int i = 1; i <= min(gridX - 1, 8 - gridY); i++) {
            if (board[grid2Num(gridX - i, gridY + i)].equals("empty")) {
                possibleGrids.add(grid2Num(gridX - i, gridY + i));
            } else if (isOccupiedByOpponentPiece(gridX - i, gridY + i, isWhite)) {
                possibleGrids.add(grid2Num(gridX - i, gridY + i));
                AttackRelation tempRelation = new AttackRelation();
                tempRelation.isWhite = isWhite;
                tempRelation.fromGridInNum = grid2Num(gridX, gridY);
                tempRelation.fromPiece = bOrQ;
                tempRelation.toGridInNum = grid2Num(gridX - i, gridY + i);
                tempRelation.toPiece = board[grid2Num(gridX - i, gridY + i)].charAt(0);
                attackRelations.add(tempRelation);
                break;// meaning there is no need to search further
            } else if (isOccupiedByOwnSidePiece(gridX - i, gridY + i, isWhite)) {
                ;
                break;
            }
        }
        return possibleGrids;
    }

    // for the QUEENS
    List<Integer> getQueenPossibleMoves(int gridX, int gridY, boolean isWhite) {
        List<Integer> possibleGrids = new ArrayList<>();
        possibleGrids.addAll(getRookPossibleMoves(gridX, gridY, isWhite, 'q'));
        possibleGrids.addAll(getBishopPossibleMoves(gridX, gridY, isWhite, 'q'));
        return possibleGrids;
    }

    List<Integer> getPawnPossibleMoves(int gridX, int gridY, boolean isWhite) {
        List<Integer> possibleGrids = new ArrayList<>();
        // for pawns things are a little different, 'cause it can only move forward
        // and for the moment 'En passant' is not supported(不支持 吃过路兵)
        if (isWhite) {
            if (gridY == 2) {// if it is the first move
                if (board[grid2Num(gridX, 3)].equals("empty")) {
                    possibleGrids.add(grid2Num(gridX, 3)); // can move forward one step
                    if (board[grid2Num(gridX, 4)].equals("empty")) { // move forward two steps if all are empty
                        possibleGrids.add(grid2Num(gridX, 4));
                    }
                }
            } else if (gridY < 8 && board[grid2Num(gridX, gridY + 1)].equals("empty")) { // as pawns cannot attack oppenent piece in the front grid
                possibleGrids.add(grid2Num(gridX, gridY + 1));
            }
            // attack left or right diagnal grid
            if (gridX - 1 > 0 && gridY + 1 < 9 && isOccupiedByBlack(gridX - 1, gridY + 1)) {
                possibleGrids.add(grid2Num(gridX - 1, gridY + 1));
                AttackRelation tempRelation = new AttackRelation();
                tempRelation.isWhite = isWhite;
                tempRelation.fromGridInNum = grid2Num(gridX, gridY);
                tempRelation.fromPiece = 'p';
                tempRelation.toGridInNum = grid2Num(gridX - 1, gridY + 1);
                tempRelation.toPiece = board[grid2Num(gridX - 1, gridY + 1)].charAt(0);
                attackRelations.add(tempRelation);
            }
            if (gridX + 1 < 9 && gridY + 1 < 9 && isOccupiedByBlack(gridX + 1, gridY + 1)) {
                possibleGrids.add(grid2Num(gridX + 1, gridY + 1));
                AttackRelation tempRelation = new AttackRelation();
                tempRelation.isWhite = isWhite;
                tempRelation.fromGridInNum = grid2Num(gridX, gridY);
                tempRelation.fromPiece = 'p';
                tempRelation.toGridInNum = grid2Num(gridX + 1, gridY + 1);
                tempRelation.toPiece = board[grid2Num(gridX + 1, gridY + 1)].charAt(0);
                attackRelations.add(tempRelation);
            }
        } else if (!isWhite) { //  all black pawns are moving downwards
            if (gridY == 7) {// if it is the first move
                if (board[grid2Num(gridX, 6)].equals("empty")) {
                    possibleGrids.add(grid2Num(gridX, 6));
                    if (board[grid2Num(gridX, 5)].equals("empty")) {
                        possibleGrids.add(grid2Num(gridX, 5));
                    }
                }
            } else if (gridY > 1 && board[grid2Num(gridX, gridY - 1)].equals("empty")) {
                possibleGrids.add(grid2Num(gridX, gridY - 1));
            }
            // attack left and right diagnal grid
            if (gridX - 1 > 0 && gridY - 1 > 0 && isOccupiedByWhite(gridX - 1, gridY - 1)) {
                possibleGrids.add(grid2Num(gridX - 1, gridY - 1));
                AttackRelation tempRelation = new AttackRelation();
                tempRelation.isWhite = isWhite;
                tempRelation.fromGridInNum = grid2Num(gridX, gridY);
                tempRelation.fromPiece = 'p';
                tempRelation.toGridInNum = grid2Num(gridX - 1, gridY - 1);
                tempRelation.toPiece = board[grid2Num(gridX - 1, gridY - 1)].charAt(0);
                attackRelations.add(tempRelation);
            }
            if (gridX + 1 < 9 && gridY - 1 > 0 && isOccupiedByWhite(gridX + 1, gridY - 1)) {
                possibleGrids.add(grid2Num(gridX + 1, gridY - 1));
                AttackRelation tempRelation = new AttackRelation();
                tempRelation.isWhite = isWhite;
                tempRelation.fromGridInNum = grid2Num(gridX, gridY);
                tempRelation.fromPiece = 'p';
                tempRelation.toGridInNum = grid2Num(gridX + 1, gridY - 1);
                tempRelation.toPiece = board[grid2Num(gridX + 1, gridY - 1)].charAt(0);
                attackRelations.add(tempRelation);
            }
        }
        return possibleGrids;
    }

    // note that king is a little different, Kings cannot move to grids that are in opponent's attacking space
    List<Integer> getKingMoveRanges(int gridX, int gridY, boolean isWhite) {
        List<Integer> possibleGrids = new ArrayList<>();
        if (gridX - 1 > 0 && gridY - 1 > 0 && isOccupiedByOwnSidePiece(gridX - 1, gridY - 1, isWhite)) {
            possibleGrids.add(grid2Num(gridX - 1, gridY - 1));
        }
        if (gridY - 1 > 0 && isOccupiedByOwnSidePiece(gridX, gridY - 1, isWhite)) {
            possibleGrids.add(grid2Num(gridX, gridY - 1));
        }
        if (gridX + 1 < 9 && gridY - 1 > 0 && isOccupiedByOwnSidePiece(gridX + 1, gridY - 1, isWhite)) {
            possibleGrids.add(grid2Num(gridX + 1, gridY - 1));
        }
        if (gridX - 1 > 0 && isOccupiedByOwnSidePiece(gridX - 1, gridY, isWhite)) {
            possibleGrids.add(grid2Num(gridX - 1, gridY));
        }
        if (gridX + 1 < 9 && isOccupiedByOwnSidePiece(gridX + 1, gridY, isWhite)) {
            possibleGrids.add(grid2Num(gridX + 1, gridY));
        }
        if (gridX - 1 > 0 && gridY + 1 < 9 && isOccupiedByOwnSidePiece(gridX - 1, gridY + 1, isWhite)) {
            possibleGrids.add(grid2Num(gridX + 1, gridY - 1));
        }
        if (gridY + 1 < 9 && isOccupiedByOwnSidePiece(gridX, gridY + 1, isWhite)) {
            possibleGrids.add(grid2Num(gridX, gridY + 1));
        }
        if (gridX + 1 < 9 && gridY + 1 < 9 && isOccupiedByOwnSidePiece(gridX + 1, gridY + 1, isWhite)) {
            possibleGrids.add(grid2Num(gridX + 1, gridY + 1));
        }
        return possibleGrids;
    }

    boolean isInOpponentAttackingRange(int gridX, int gridY, Set<Integer> moveRanges) {
        return moveRanges.contains(grid2Num(gridX, gridY));
    }

    Set<Integer> getAttackingSpace(boolean isWhite) { // if isWhite = true means white pieces are attacking
        Set<Integer> attackingSpace = new HashSet<>();
        for (int i = 0; i < 64; i++) {
            // go throught all opponent's possible moves
            char piece = board[i].charAt(0);
            if (isWhite) {
                if (piece == 'P') {
                    attackingSpace.addAll(getPawnPossibleMoves(num2CoordX(i), num2CoordY(i), true));
                }
                if (piece == 'R') {
                    attackingSpace.addAll(getRookPossibleMoves(num2CoordX(i), num2CoordY(i), true, 'r'));
                }
                if (piece == 'N') {
                    attackingSpace.addAll(getKnightPossibleMoves(num2CoordX(i), num2CoordY(i), true));
                }
                if (piece == 'B') {
                    attackingSpace.addAll(getBishopPossibleMoves(num2CoordX(i), num2CoordY(i), true, 'b'));
                }
                if (piece == 'Q') {
                    attackingSpace.addAll(getQueenPossibleMoves(num2CoordX(i), num2CoordY(i), true));
                }
                if (piece == 'K') {
                    attackingSpace.addAll(getKingMoveRanges(num2CoordX(i), num2CoordY(i), true));
                }
                whiteAttackingSpace = attackingSpace;
            } else if (!isWhite) {
                if (piece == 'p') {
                    attackingSpace.addAll(getPawnPossibleMoves(num2CoordX(i), num2CoordY(i), false));
                }
                if (piece == 'r') {
                    attackingSpace.addAll(getRookPossibleMoves(num2CoordX(i), num2CoordY(i), false, 'r'));
                }
                if (piece == 'n') {
                    attackingSpace.addAll(getKnightPossibleMoves(num2CoordX(i), num2CoordY(i), false));
                }
                if (piece == 'b') {
                    attackingSpace.addAll(getBishopPossibleMoves(num2CoordX(i), num2CoordY(i), false, 'b'));
                }
                if (piece == 'q') {
                    attackingSpace.addAll(getQueenPossibleMoves(num2CoordX(i), num2CoordY(i), false));
                }
                if (piece == 'k') {
                    attackingSpace.addAll(getKingMoveRanges(num2CoordX(i), num2CoordY(i), false));
                }
                blackAttackSpace = attackingSpace;
            }
        }
        return attackingSpace;
    }

    List<Integer> getKingPossibleMoves(int gridX, int gridY, boolean isWhite) {
        List<Integer> possibleGrids = new ArrayList<>();
        Set<Integer> attackedGrids = getAttackingSpace(!isWhite);

        // there are 8 grids the king can go but not occupied by its own pieces and not attacked by any of the opponents' pieces
        // check if it is checkmated
        if (isWhite && isInOpponentAttackingRange(gridX, gridY, attackedGrids)) {
            isWhiteCheckmated = true;
        } else if (!isWhite && isInOpponentAttackingRange(gridX, gridY, attackedGrids)) {
            isBlackCheckmated = true;
        }
        //
        if (gridX - 1 > 0 && gridY - 1 > 0 && !isOccupiedByOwnSidePiece(gridX - 1, gridY - 1, isWhite) && isInOpponentAttackingRange(gridX - 1, gridY - 1, attackedGrids)) {
            possibleGrids.add(grid2Num(gridX - 1, gridY - 1));
        }
        if (gridY - 1 > 0 && !isOccupiedByOwnSidePiece(gridX, gridY - 1, isWhite) && isInOpponentAttackingRange(gridX, gridY - 1, attackedGrids)) {
            possibleGrids.add(grid2Num(gridX, gridY - 1));
        }
        if (gridX + 1 < 9 && gridY - 1 > 0 && !isOccupiedByOwnSidePiece(gridX + 1, gridY - 1, isWhite) && isInOpponentAttackingRange(gridX + 1, gridY - 1, attackedGrids)) {
            possibleGrids.add(grid2Num(gridX + 1, gridY - 1));
        }
        if (gridX - 1 > 0 && !isOccupiedByOwnSidePiece(gridX - 1, gridY, isWhite) && isInOpponentAttackingRange(gridX - 1, gridY, attackedGrids)) {
            possibleGrids.add(grid2Num(gridX - 1, gridY));
        }
        if (gridX + 1 < 9 && !isOccupiedByOwnSidePiece(gridX + 1, gridY, isWhite) && isInOpponentAttackingRange(gridX + 1, gridY, attackedGrids)) {
            possibleGrids.add(grid2Num(gridX + 1, gridY));
        }
        if (gridX - 1 > 0 && gridY + 1 < 9 && !isOccupiedByOwnSidePiece(gridX - 1, gridY + 1, isWhite) && isInOpponentAttackingRange(gridX - 1, gridY + 1, attackedGrids)) {
            possibleGrids.add(grid2Num(gridX + 1, gridY - 1));
        }
        if (gridY + 1 < 9 && !isOccupiedByOwnSidePiece(gridX, gridY + 1, isWhite) && isInOpponentAttackingRange(gridX, gridY + 1, attackedGrids)) {
            possibleGrids.add(grid2Num(gridX, gridY + 1));
        }
        if (gridX + 1 < 9 && gridY + 1 < 9 && !isOccupiedByOwnSidePiece(gridX + 1, gridY + 1, isWhite) && isInOpponentAttackingRange(gridX + 1, gridY + 1, attackedGrids)) {
            possibleGrids.add(grid2Num(gridX + 1, gridY + 1));
        }
        // castling(王车易位)
        //todo


        return possibleGrids;
    }


    /* this function is only valid after attackRelations has been calculated
     */
    boolean isAttacked(int gridNum, boolean isWhite) { // isWhite stands for the one attacking
        // this
        Set<Integer> attackedGrids = getAttackingSpace(!isWhite);
        if (attackedGrids.contains(gridNum)) {
            return true;
        } else {
            return false;
        }
    }

    boolean ifPieceIsWhite(char piece) {
        if (piece == 'p' || piece == 'n' || piece == 'b' || piece == 'r' || piece == 'q' || piece == 'k') {
            return false;
        } else if (piece == 'P' || piece == 'N' || piece == 'B' || piece == 'R' || piece == 'Q' || piece == 'K') {
            return true;
        }
        return false;
    }

    int calScore(char piece, int fromNum, int toNum, char toPostionAttacks) {
    /*
      rules: pawn attacking  +10, attacked -10;
      knight +30 -30; bishop +30 -30; rook: +50 -50
      queen +90 -90; king +900 -900
    */
        //
        int score = 0;
        boolean fromPositionIsAttacked = ifPieceIsWhite(piece) ?
                isInOpponentAttackingRange(num2CoordX(fromNum), num2CoordY(fromNum), blackAttackSpace) :
                isInOpponentAttackingRange(num2CoordX(fromNum), num2CoordY(fromNum), whiteAttackingSpace);
        boolean toPositionIsAttacked = ifPieceIsWhite(piece) ?
                isInOpponentAttackingRange(num2CoordX(toNum), num2CoordY(toNum), blackAttackSpace) :
                isInOpponentAttackingRange(num2CoordX(toNum), num2CoordY(toNum), whiteAttackingSpace);
        if (fromPositionIsAttacked) { // because making the move avoid being attacked so it is a positive action
            if (piece == 'p' || piece == 'P') score += 10;
            if (piece == 'n' || piece == 'N') score += 30;
            if (piece == 'b' || piece == 'B') score += 30;
            if (piece == 'r' || piece == 'R') score += 50;
            if (piece == 'q' || piece == 'Q') score += 90;
            if (piece == 'k' || piece == 'K') score += 900;
        }
        if (toPositionIsAttacked) {
            if (piece == 'p' || piece == 'P') score += -10;
            if (piece == 'n' || piece == 'N') score += -30;
            if (piece == 'b' || piece == 'B') score += -30;
            if (piece == 'r' || piece == 'R') score += -50;
            if (piece == 'q' || piece == 'Q') score += -90;
            if (piece == 'k' || piece == 'K') score += -900;
        }
        if (toPostionAttacks == 'p' || toPostionAttacks == 'P') score += 10;
        if (toPostionAttacks == 'n' || toPostionAttacks == 'N') score += 30;
        if (toPostionAttacks == 'b' || toPostionAttacks == 'B') score += 30;
        if (toPostionAttacks == 'r' || toPostionAttacks == 'R') score += 50;
        if (toPostionAttacks == 'q' || toPostionAttacks == 'Q') score += 90;
        if (toPostionAttacks == 'k' || toPostionAttacks == 'K') score += 900;
        return score;
    }

    List<Move> tempTo2Moves(char piece, int fromNum, List<Integer> toNums) {
        List<Move> tempMoves = new ArrayList<>();
        for (int i = 0; i < toNums.size(); i++) {
            Move tempMove = new Move();
            tempMove.fromGridNum = fromNum;
            tempMove.toGridNum = toNums.get(i);
            tempMove.type = piece;
            tempMoves.add(tempMove);
        }
        return tempMoves;
    }

    List<Move> getAllMoves(boolean isWhite) {
        List<Move> tempMoves = new ArrayList<>();
        for (int i = 0; i < 64; i++) {
            char piece = board[i].charAt(0);
            if (piece == (isWhite ? 'P' : 'p')) {
                tempMoves.addAll(tempTo2Moves(piece, i, getPawnPossibleMoves(num2CoordX(i), num2CoordY(i), isWhite)));
            }
            if (piece == (isWhite ? 'R' : 'r')) {
                tempMoves.addAll(tempTo2Moves(piece, i, getRookPossibleMoves(num2CoordX(i), num2CoordY(i), isWhite, 'r')));
            }
            if (piece == (isWhite ? 'N' : 'n')) {
                tempMoves.addAll(tempTo2Moves(piece, i, getKnightPossibleMoves(num2CoordX(i), num2CoordY(i), isWhite)));
            }
            if (piece == (isWhite ? 'B' : 'b')) {
                tempMoves.addAll(tempTo2Moves(piece, i, getBishopPossibleMoves(num2CoordX(i), num2CoordY(i), isWhite, 'b')));
            }
            if (piece == (isWhite ? 'Q' : 'q')) {
                tempMoves.addAll(tempTo2Moves(piece, i, getQueenPossibleMoves(num2CoordX(i), num2CoordY(i), isWhite)));
            }
            if (piece == (isWhite ? 'K' : 'k')) {
                tempMoves.addAll(tempTo2Moves(piece, i, getKingPossibleMoves(num2CoordX(i), num2CoordY(i), isWhite)));
            }
        }
        return tempMoves;
    }

    // evaluate the current situation and return the best move
    Move getBestMove(boolean isWhite) {
        int score = -9999;
        Move bestMove = new Move();
        List<Move> tempMoves = getAllMoves(isWhite);

        if (tempMoves.size() < 1 && (isWhite ? isWhiteCheckmated : isBlackCheckmated)) {
            System.out.println("game over..." + (isWhite ? "White side win!!!" : "Black side win!!!"));
        }
        for (int i = 0; i < tempMoves.size(); i++) {
            boolean fromIsAttacked = isAttacked(tempMoves.get(i).fromGridNum, isWhite); // meaning black is attacked
            boolean toGridIsAttacked = isAttacked(tempMoves.get(i).toGridNum, isWhite);
            char targetGridPiece = board[tempMoves.get(i).toGridNum].charAt(0);
            int tempScore = calScore(tempMoves.get(i).type, tempMoves.get(i).fromGridNum, tempMoves.get(i).toGridNum, targetGridPiece);
            if (score < tempScore) {
                bestMove.isWhite = isWhite;
                bestMove.fromGridNum = tempMoves.get(i).fromGridNum;
                bestMove.toGridNum = tempMoves.get(i).toGridNum;
                bestMove.type = tempMoves.get(i).type;
                score = tempScore;
            }
        }
        return bestMove;
    }
// dicide if it is win or lose

    //
    void updateBoard(int fromX, int fromY, int toX, int toY) {
        board[grid2Num(toX, toY)] =board[grid2Num(fromX, fromY)];
        board[grid2Num(fromX, fromY)] ="empty";
    }
    int iPawn = 0;
    void promotePawn(int fromNum,int toNum,String prmotedPiece) {
        board[toNum] = prmotedPiece;
        board[fromNum] ="empty";
    }

    int min(int x, int y) {
        if (x > y) {
            return y;
        } else {
            return x;
        }
    }
    void printBoard(){
        System.out.println("    a b c d e f g h ");
        System.out.println("________________________");

        for (int i = 0; i <board.length; i++) {
            int iY = 8 - (int)i/8;
            if(i%8==0) System.out.print(iY+" | ");
            System.out.print(board[i].charAt(0)=='e'?"  ":board[i].charAt(0)+" ");
            if(i%8==7) System.out.println(" | "+iY);
        }
        System.out.println("------------------------");
        System.out.println("    1 2 3 4 5 6 7 8 ");

    }
    void printBoardTest(){
        System.out.println("    a b c d e f g h ");
        System.out.println("________________________");

        for (int i = 0; i <board.length; i++) {
            int iY = 8 - (int)i/8;
            if(i%8==0) System.out.print(iY+" | ");
            System.out.print((i<10?("0"+i):i)+" ");
            if(i%8==7) System.out.println(" | "+iY);
        }
        System.out.println("------------------------");
        System.out.println("    01 02 03 04 05 06 07 08 ");

    }
    void printPossibleMoves(List<Integer> moves,int gridX,int gridY){
        System.out.println("    a b c d e f g h ");
        System.out.println("________________________");
        char[] tempBoard = new char[64];
        for (int i = 0; i < 64; i++) {
            tempBoard[i] = board[i].charAt(0);
        }
        for (int i = 0; i < moves.size(); i++) {
            tempBoard[moves.get(i)] = '@';
        }
        tempBoard[grid2Num(gridX,gridY)] = '*';
        for (int i = 0; i <tempBoard.length; i++) {
            int iY = 8 - (int)i/8;
            if(i%8==0) System.out.print(iY+" | ");
            System.out.print(tempBoard[i]=='e'?"  ":tempBoard[i]+" ");
            if(i%8==7) System.out.println(" | "+iY);
        }
        System.out.println("------------------------");
        System.out.println("    1 2 3 4 5 6 7 8 ");

    }
    public static void main(String[] args) {
        Chess chess = new Chess();
        chess.printBoard();
//        for (int i = 0; i <8 ; i++) {
//            System.out.println( chess.num2CoordX(i)+", "+chess.num2CoordY(i)+" : "+i);
//        }
//        System.out.println(chess.grid2Num(5,5));
        Move move = chess.getBestMove(true);
        List<Integer> moves= chess.getPawnPossibleMoves(chess.num2CoordX(move.fromGridNum),chess.num2CoordY(move.fromGridNum),true);
        chess.printPossibleMoves(moves,1,2);
        String testFen = "rnbqkbnr/pp1ppppp/8/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2";
        chess.board = chess.forsythEdwardsNotation2Board(testFen);
        chess.printBoard();
    }

    boolean run(int fromNum,int toNum){
        // check if the piece moved is controled by users not by computers
        if(isOccupiedByWhite(num2CoordX(fromNum),num2CoordY(fromNum))&&!isOccupiedByBlack(num2CoordX(fromNum),num2CoordY(fromNum))){
            return false;
        }
        // by defualt, the user is
        if (checkMoveIsValid(fromNum, toNum)){
            Move move = getBestMove(false);
            updateBoard(num2CoordX(move.fromGridNum),num2CoordY(move.fromGridNum),num2CoordX(move.toGridNum),num2CoordX(move.toGridNum));
            return true;
        }else{
            System.out.println("error");
            return false;
        }
    }
    boolean checkMoveIsValid(int fromNum, int toNum){
        char piece = board[fromNum].charAt(0);
        List<Integer> possibles = new ArrayList<>();
        if (piece == 'p') possibles = getPawnPossibleMoves(num2CoordX(fromNum),num2CoordY(fromNum),false);
        if (piece == 'r') possibles = getRookPossibleMoves(num2CoordX(fromNum),num2CoordY(fromNum),false,'r');
        if (piece == 'n') possibles = getKnightPossibleMoves(num2CoordX(fromNum),num2CoordY(fromNum),false);
        if (piece == 'b') possibles = getBishopPossibleMoves(num2CoordX(fromNum),num2CoordY(fromNum),false,'b');
        if (piece == 'q') possibles = getQueenPossibleMoves(num2CoordX(fromNum),num2CoordY(fromNum),false);
        if (piece == 'k') possibles = getKingPossibleMoves(num2CoordX(fromNum),num2CoordY(fromNum),false);

        if (piece == 'P') possibles = getPawnPossibleMoves(num2CoordX(fromNum),num2CoordY(fromNum),true);
        if (piece == 'R') possibles = getRookPossibleMoves(num2CoordX(fromNum),num2CoordY(fromNum),true,'r');
        if (piece == 'N') possibles = getKnightPossibleMoves(num2CoordX(fromNum),num2CoordY(fromNum),true);
        if (piece == 'B') possibles = getBishopPossibleMoves(num2CoordX(fromNum),num2CoordY(fromNum),true,'b');
        if (piece == 'Q') possibles = getQueenPossibleMoves(num2CoordX(fromNum),num2CoordY(fromNum),true);
        if (piece == 'K') possibles = getKingPossibleMoves(num2CoordX(fromNum),num2CoordY(fromNum),true);
        if(possibles.contains(toNum)){
            return true;
        }else {
            return false;
        }
    }
//    福斯夫－爱德华兹记号法（Forsyth–Edwards Notation），简称FEN，是苏格兰人David Forsyth发明的国际象棋可完整叙述局面的记谱法，也可用于象棋
//    使用ASCII字符串代码，代码意义依次是：
//    棋子位置数值区域（Piece placement data）：
//    按白方视角，描述由上至下、由左至右的盘面，以/符号来分隔相邻横列。白方、黑方分别以大写、小写英文字母表达兵种：P、N、B、R、Q、K分别代表士兵、骑士、主教、城堡、皇后、国王。各横列的连续空格以阿拉伯数字表示，例如5即代表连续5个空格[1]。
//    轮走棋方（Active color）：以w表示白方；b表示黑方。
//    易位可行性（Castling availability）：写KQ表示白方可易位；kq表示黑方可易位；KQkq表示两方均可易位。
//    吃过路兵目标格（En passant target square）：写走棋方若吃过路兵后会到的棋格，若无则写-。
//    半回合计数（Halfmove clock）：以阿拉伯数字表示，从最后一次吃子或移动兵开始计算的回合数，用于判断五十回合自然限著和局。
//    回合数（Fullmove number）：以阿拉伯数字表示，从开局开始计算的回合数
    String[] forsythEdwardsNotation2Board(String fen){
        String[] board = new String[64];
        String rows[] = fen.split(" ")[0].split("/");
        int iBoard = 0;
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < rows[i].length(); j++) {
                char piece = rows[i].charAt(j);
                if(Character.isDigit(piece)){
                   int iEmpty = Integer.parseInt(String.valueOf(piece));
                    for (int k = 0; k < iEmpty; k++) {
                        board[iBoard] = "empty";
                        iBoard++;
                    }
                }else{
                    System.out.println(piece);
                    board[iBoard] = String.valueOf(piece);
                    iBoard++;
                }
            }
        }
        return board;
    }
}