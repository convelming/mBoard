
// https://www.beichengjiu.com/game/170153.html
// 记谱规则
// 使用ASCII字符串代码，代码意义依次是：
// 棋子位置数值区域（Piece placement data）：
// 按白方视角，描述由上至下、由左至右的盘面，以/符号来分隔相邻横列。白方、黑方分别以大写、小写英文字母表达兵种：P、N、B、R、Q、K分别代表士兵、骑士、主教、城堡、皇后、国王。各横列的连续空格以阿拉伯数字表示，例如5即代表连续5个空格[1]。
// 轮走棋方（Active color）：以w表示白方；b表示黑方。
// 易位可行性（Castling availability）：写KQ表示白方可易位；kq表示黑方可易位；KQkq表示两方均可易位。
// 吃过路兵目标格（En passant target square）：写走棋方若吃过路兵后会到的棋格，若无则写-。
// 半回合计数（Halfmove clock）：以阿拉伯数字表示，从最后一次吃子或移动兵开始计算的回合数，用于判断五十回合自然限著和局。
// 回合数（Fullmove number）：以阿拉伯数字表示，从开局开始计算的回合数
// https://www.chessgames.com/fenhelp.html

class DueMotors
{
public:
    // motors' pin information

    int dirPin1 = 3;
    int stepperPin1 = 2;
    int dirPin2 = 7;
    int stepperPin2 = 6;
    // int topLimitSwPin; bool topLimitSw  = false;
    // int bottomLimitSwPin; bool bottomLimitSw  = false;
    // int leftLimitSwPin; bool leftLimitSw  = false;
    // int rightLimitSwPin; bool rightLimitSw  = false;
    int magnetPin = 13;

    double x = 0.0;
    double y = 0.0;
    int calThreshHold = 100;
    int iCalibrate;
    double desX;
    double desY;
    int adRange = 16000; // steps motor turns for the whole x_range
    int wsRange = 13600; // steps motor turns for the whole y_range

    double adTotalDis = 316;  //on step is about to move 0.0198mm distance
    double wsTotalDis = 272;  //on step is about to move 0.02mm distance
    double adPer = adTotalDis / adRange; 
    double wsPer = wsTotalDis / adRange;
    double gridStep = min(adTotalDis, wsTotalDis) / 8; // how many steps/turns it takes to move one grid distance
                                                       // constructor
    DueMotors(){}
    void initialize(int dirPin1,int stepperPin1,int dirPin2, int stepperPin2,// motor pins 
    //int topLimitSwPin,int bottomLimitSwPin,int leftLimitSwPin, int rightLimitSwPin, // limit switches pin setup
    int magnetPin // magnet singal pin
    ){
        dirPin1 = dirPin1;
        stepperPin1 = stepperPin1;
        dirPin2 = dirPin2;
        stepperPin2 = stepperPin2;
        // adRange = hSteps; // steps motor turns for the whole x_range
        // wsRange = vSteps; // steps motor turns for the whole y_range
        // topLimitSwPin = topLimitSwPin;
        // bottomLimitSwPin = bottomLimitSwPin;
        // leftLimitSwPin = leftLimitSwPin;
        // rightLimitSwPin = rightLimitSwPin;
        magnetPin = magnetPin;  

        // pinMode(topLimitSwPin, INPUT_PULLUP); //limit switch input
        // pinMode(bottomLimitSwPin, INPUT_PULLUP);
        // pinMode(leftLimitSwPin, INPUT_PULLUP);
        // pinMode(rightLimitSwPin, INPUT_PULLUP);

        // relay switch with magnet
        pinMode(magnetPin, OUTPUT);
        // reset magnet position
        calibrate();
        moveTo('d',4);
    }
    
    void calibrate(){
        // while(!bottomLimitSw){
        //     moveDown(10);
        //     bottomLimitSw = digitalRead(bottomLimitSwPin);
        // }
        // while (!leftLimitSw){
        //     moveLeft(10);
        //     leftLimitSw = digitalRead(leftLimitSwPin);
        // }

        moveLeft( x / adPer + 100);
        moveDown(y / adPer + 100);
        x = 0.0;
        y = 0.0;
        iCalibrate = 0;
    }

    void moveTo(char k, int n)
    {
       magnetOn();
        // move to grid edge first
        gridPos2Coord(k, n);
        double stepX = desX - x;
        double stepY = desY - y;
        if (stepX > 0 && stepY > 0)
        {
            moveUpRight(gridStep / 2);
            moveUp(stepY - gridStep);
            moveRight(stepX - gridStep);
            moveUpRight(gridStep / 2);
        }
        else if (stepX > 0 && stepY < 0)
        {
            moveDownRight(gridStep / 2);
            moveDown(stepY - gridStep);
            moveRight(-stepX - gridStep);
            moveDownRight(gridStep / 2);
        }
        else if (stepX < 0 && stepY > 0)
        {
            moveUpLeft(gridstep / 2);
            moveUp(-stepY - gridStep);
            moveLeft(stepX - gridStep);
            moveUpLeft(gridStep / 2);
        }
        else if (stepX < 0 && stepY < 0)
        {
            moveDownLeft(gridStep / 2);
            moveDown(-stepY - gridStep);
            moveLeft(-stepX - gridStep);
            moveDownLeft(gridstep / 2);
        }
        else if (stepX == 0 && stepY > 0)
        {
            moveUpRight(gridStep / 2);
            moveUp(stepY - gridStep);
            moveUpLeft(gridStep / 2);
        }
        else if (stepX == 0 && stepY < 0)
        {
            moveDownRight(gridStep / 2);
            moveDown(-stepY - gridStep);
            moveDownLeft(gridStep / 2);
        }
        else if (stepY == 0 && stepX > 0)
        {
            moveUpRight(gridStep / 2);
            moveRight(stepX - gridStep);
            moveUpLeft(gridStep / 2);
        }
        else if (stepY == 0 && stepX < 0)
        {
            moveUpLeft(gridStep / 2);
            moveLeft(-stepX - gridStep);
            moveDownLeft(gridStep / 2);
        }
        magnetOff();
        // update magnet position
        x = desX;
        y = desY;
        iCalibrate++;
        if(iCalibrate>calThreshHold){
            calibrate();
        };
    }

    void step(boolean dir1, boolean dir2, double steps)
    {
        digitalWrite(dirPin1, dir1);
        digitalWrite(dirPin2, dir2);
        delay(50);
        for (int i = 0; i < steps; i++)
        {
            digitalWrite(stepperPin1, HIGH);
            digitalWrite(stepperPin2, HIGH);
            delayMicroseconds(200);
            digitalWrite(stepperPin1, LOW);
            digitalWrite(stepperPin2, LOW);
            delayMicroseconds(200);
        }
    }

    void moveUp(double steps)
    {
        step(false, true, steps);
    }
    void moveDown(double steps)
    {
        step(true, false, steps);
    }
    void moveLeft(double steps)
    {
        step(true, true, steps);
    }
    void moveRight(double steps)
    {
        step(false, false, steps);
    }
    void moveUpLeft(double steps)
    {
        digitalWrite(dirPin2, true);
        delay(50);
        for (int i = 0; i < steps; i++)
        {
            digitalWrite(stepperPin2, HIGH);
            delayMicroseconds(200);
            digitalWrite(stepperPin2, LOW);
            delayMicroseconds(200);
        }
    }
    void moveUpRight(double steps)
    {
        digitalWrite(dirPin1, false);
        delay(50);
        for (int i = 0; i < steps; i++)
        {
            digitalWrite(stepperPin1, HIGH);
            delayMicroseconds(200);
            digitalWrite(stepperPin1, LOW);
            delayMicroseconds(200);
        }
    }
    void moveDownLeft(double steps)
    {
        digitalWrite(dirPin1, true);
        delay(50);
        for (int i = 0; i < steps; i++)
        {
            digitalWrite(stepperPin1, HIGH);
            delayMicroseconds(200);
            digitalWrite(stepperPin1, LOW);
            delayMicroseconds(200);
        }
    }
    void moveDownRight(double steps)
    {
        digitalWrite(dirPin2, false);
        delay(50);
        for (int i = 0; i < steps; i++)
        {
            digitalWrite(stepperPin2, HIGH);
            delayMicroseconds(200);
            digitalWrite(stepperPin2, LOW);
            delayMicroseconds(200);
        }
    }

    double min(double a, double b)
    {
        if (a > b)
        {
            return b;
        } else
        {
            return a;
        }
    }
    int char2Int(char k)
    {
        if(k=='a') return 1;
        if(k=='b') return 2;
        if(k=='c') return 3;
        if(k=='d') return 4;
        if(k=='e') return 5;
        if(k=='f') return 6;
        if(k=='g') return 7;
        if(k=='h') return 8;
        if(k=='i') return 9;
       
    }
    char int2Char(int n){
        if(n==1) return 'a';
        if(n==2) return 'b';
        if(n==3) return 'c';
        if(n==4) return 'd';
        if(n==5) return 'e';
        if(n==6) return 'f';
        if(n==7) return 'g';
        if(n==8) return 'h';
        if(n==9) return 'i';
        
    }
    void gridPos2Coord(char k, int n)
    {
        int m = char2Int(k);
        desX = m * adPer;
        desY = n * wsPer;
    }
    char getXGridPos(double x){
        int tempX = x / adPer;
        return int2Char(tempX);
    }
    int getYGridPos(double y){
        int tempY = y / wsPer; 
        return tempY;
    }

    // control magnet
    void magnetOn(){
        digitalWrite(magnetPin,HIGH);
    }
    void magnetOff(){
        digitalWrite(magnetPin,LOW);
    }

    void test(){
        Serial.print("current pos: ");Serial.print(getXGridPos(x));Serial.print(" ");Serial.println(getYGridPos(y));
        for (int  i = 0; i < calThreshHold-1; i++)
        {
            /* code */
            char posX = int2Char(random(8)+1);
            int posY = random(7)+1;
            moveTo(posX,posY);
            Serial.println(i);
            Serial.print ("moved to:"); Serial.print(posX);Serial.print(" "); Serial.println(posY);
        }
        Serial.print("done!! check how much errors are there after 99 moves");

    }
}