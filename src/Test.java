public class Test {
    public static void main(String[] args) {
        helloWorld();
        Chess chess = new Chess();
        System.out.println(chess.board[0]);
    }
    public static void helloWorld(){
        System.out.println("hello world!");
    }
    public static void test(){
        double xMin = Double.MAX_VALUE;
        double x = 0;
        int m = 0;
        int x1 = -1; int x2 = -1;
        int x3 = -1; int x4 = -1;
        for (int i = 1;i<4000;i++){
            for (int j = 1; j <= 4000; j++) {
                for (int k = 1; k <= 4000; k++) {
                    m = 4000-i-j-k;
                    if(m>=0) {
                        x = (i + k) / 400 * (i + k) + 2 * (k + m) + 15 * (i + m + j + m) + (j + k) / 400 * (j + k);
                        if(xMin>=x){
                            xMin = x;
                            x1 = i;x2=j;x3=m;x4=k;
                        }

                    }
                }
            }
        }
        System.out.println("best: "+xMin+" with x1 = "+x1+", x2="+x2+", x3="+x3+", x4="+x4);
    }
}
