#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* =========================
   棋子编码（4 bit / 格）
   =========================
   bit3 : 颜色 (0=白,1=黑)
   bit2-0 : 类型
*/
#define EMPTY  0
#define PAWN   1
#define KNIGHT 2
#define BISHOP 3
#define ROOK   4
#define QUEEN  5
#define KING   6

#define BLACK  0x8

uint8_t board[32];   // 64 格 × 4 bit = 32 bytes
uint8_t sideToMove;  // 0=white, 1=black

/* =========================
   Board 访问宏
   ========================= */
#define GET(sq)   (((sq)&1)? (board[(sq)>>1]>>4) : (board[(sq)>>1]&0xF))
#define SET(sq,v) do{ \
  if((sq)&1) board[(sq)>>1]=(board[(sq)>>1]&0x0F)|((v)<<4); \
  else       board[(sq)>>1]=(board[(sq)>>1]&0xF0)|(v); \
}while(0)

#define IS_BLACK(p) ((p)&BLACK)
#define TYPE(p)     ((p)&7)

/* =========================
   FEN 解析 / 输出
   ========================= */
void load_fen(const char *fen){
  memset(board,0,sizeof(board));
  int sq=0;
  while(*fen && *fen!=' '){
    char c=*fen++;
    if(c=='/') continue;
    if(c>='1'&&c<='8'){
      sq+=c-'0';
    }else{
      uint8_t p=0;
      if(c>='a'){ p|=BLACK; c-=32; }
      switch(c){
        case 'P':p|=PAWN;break;
        case 'N':p|=KNIGHT;break;
        case 'B':p|=BISHOP;break;
        case 'R':p|=ROOK;break;
        case 'Q':p|=QUEEN;break;
        case 'K':p|=KING;break;
      }
      SET(sq++,p);
    }
  }
  fen++;
  sideToMove = (*fen=='b');
}

void print_fen(){
  int empty=0;
  for(int i=0;i<64;i++){
    uint8_t p=GET(i);
    if(!p) empty++;
    else{
      if(empty){ printf("%d",empty); empty=0; }
      char c=" PNBRQK"[TYPE(p)];
      if(IS_BLACK(p)) c+=32;
      printf("%c",c);
    }
    if(i%8==7){
      if(empty){ printf("%d",empty); empty=0; }
      if(i!=63) printf("/");
    }
  }
  printf(" %c\n",sideToMove?'b':'w');
}

/* =========================
   攻击 / 将军
   ========================= */
int attacked(int sq,int byBlack){
  int x=sq%8,y=sq/8;
  int dx[8]={1,1,1,0,0,-1,-1,-1};
  int dy[8]={1,0,-1,1,-1,1,0,-1};

  /* 直线/斜线 */
  for(int d=0;d<8;d++){
    int nx=x+dx[d],ny=y+dy[d];
    while(nx>=0&&nx<8&&ny>=0&&ny<8){
      uint8_t p=GET(ny*8+nx);
      if(p){
        if(IS_BLACK(p)==byBlack){
          int t=TYPE(p);
          if(t==QUEEN ||
             (t==ROOK   && (d%2)) ||
             (t==BISHOP && !(d%2)))
            return 1;
        }
        break;
      }
      nx+=dx[d]; ny+=dy[d];
    }
  }

  /* 马 */
  int kx[8]={2,2,1,1,-1,-1,-2,-2};
  int ky[8]={1,-1,2,-2,2,-2,1,-1};
  for(int i=0;i<8;i++){
    int nx=x+kx[i],ny=y+ky[i];
    if(nx>=0&&nx<8&&ny>=0&&ny<8){
      uint8_t p=GET(ny*8+nx);
      if(p&&TYPE(p)==KNIGHT&&IS_BLACK(p)==byBlack) return 1;
    }
  }
  return 0;
}

int findKing(int black){
  for(int i=0;i<64;i++){
    uint8_t p=GET(i);
    if(p&&TYPE(p)==KING&&IS_BLACK(p)==black) return i;
  }
  return -1;
}

int inCheck(int black){
  return attacked(findKing(black),!black);
}

/* =========================
   走法生成
   ========================= */
int genMoves(uint8_t mv[][2]){
  int n=0;
  int dx[8]={1,1,1,0,0,-1,-1,-1};
  int dy[8]={1,0,-1,1,-1,1,0,-1};

  for(int i=0;i<64;i++){
    uint8_t p=GET(i);
    if(!p||IS_BLACK(p)!=sideToMove) continue;
    int x=i%8,y=i/8;

    /* 兵 */
    if(TYPE(p)==PAWN){
      int d=sideToMove?1:-1;
      int ny=y+d;
      if(ny>=0&&ny<8){
        int f=ny*8+x;
        if(!GET(f)){
          mv[n][0]=i; mv[n++][1]=f;
        }
        if(x>0){
          int c=f-1;
          if(GET(c)&&IS_BLACK(GET(c))!=sideToMove)
            mv[n][0]=i,mv[n++][1]=c;
        }
        if(x<7){
          int c=f+1;
          if(GET(c)&&IS_BLACK(GET(c))!=sideToMove)
            mv[n][0]=i,mv[n++][1]=c;
        }
      }
      continue;
    }

    /* 马 */
    if(TYPE(p)==KNIGHT){
      int kx[8]={2,2,1,1,-1,-1,-2,-2};
      int ky[8]={1,-1,2,-2,2,-2,1,-1};
      for(int j=0;j<8;j++){
        int nx=x+kx[j],ny=y+ky[j];
        if(nx>=0&&nx<8&&ny>=0&&ny<8){
          int s=ny*8+nx;
          if(!GET(s)||IS_BLACK(GET(s))!=sideToMove)
            mv[n][0]=i,mv[n++][1]=s;
        }
      }
      continue;
    }

    /* 王 */
    if(TYPE(p)==KING){
      for(int d=0;d<8;d++){
        int nx=x+dx[d],ny=y+dy[d];
        if(nx>=0&&nx<8&&ny>=0&&ny<8){
          int s=ny*8+nx;
          if(!GET(s)||IS_BLACK(GET(s))!=sideToMove)
            mv[n][0]=i,mv[n++][1]=s;
        }
      }
      continue;
    }

    /* 滑子：象 / 车 / 后 */
    for(int d=0;d<8;d++){
      if(TYPE(p)==ROOK   && !(d%2)) continue;
      if(TYPE(p)==BISHOP &&  (d%2)) continue;
      int nx=x+dx[d],ny=y+dy[d];
      while(nx>=0&&nx<8&&ny>=0&&ny<8){
        int s=ny*8+nx;
        if(GET(s)){
          if(IS_BLACK(GET(s))!=sideToMove)
            mv[n][0]=i,mv[n++][1]=s;
          break;
        }
        mv[n][0]=i,mv[n++][1]=s;
        nx+=dx[d]; ny+=dy[d];
      }
    }
  }
  return n;
}

/* =========================
   评估函数
   ========================= */
int eval(){
  int v=0;
  for(int i=0;i<64;i++){
    uint8_t p=GET(i);
    if(!p) continue;
    int t=TYPE(p);
    int s=(t==PAWN)?100:
          (t==KNIGHT)?320:
          (t==BISHOP)?330:
          (t==ROOK)?500:
          (t==QUEEN)?900:0;
    v+=IS_BLACK(p)?-s:s;
  }
  return v;
}

/* =========================
   Alpha-Beta
   ========================= */
int alphabeta(int d,int a,int b){
  if(!d) return eval();
  uint8_t mv[128][2];
  int n=genMoves(mv);
  if(!n) return inCheck(sideToMove)?-30000:0;

  for(int i=0;i<n;i++){
    uint8_t f=mv[i][0],t=mv[i][1],cap=GET(t);
    SET(t,GET(f)); SET(f,0); sideToMove^=1;
    int v=-alphabeta(d-1,-b,-a);
    sideToMove^=1; SET(f,GET(t)); SET(t,cap);
    if(v>=b) return b;
    if(v>a) a=v;
  }
  return a;
}

/* =========================
   将死判断
   ========================= */
int isCheckmate(){
  if(!inCheck(sideToMove)) return 0;
  uint8_t mv[128][2];
  return genMoves(mv)==0;
}

/* =========================
   示例 main（PC 测试用）
   ========================= */
int main(){
  load_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w");
  print_fen();

  int score=alphabeta(3,-30000,30000);
  printf("Eval = %d\n",score);

  printf("Checkmate? %d\n",isCheckmate());
  return 0;
}
