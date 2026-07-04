
// 项目名称：综合实验3——家用电子秤快速原型设计

#include "HX711.h"
#include "Timer.h"
#define INF 1000000000

//Below are for Linear Regression Simulation
volatile double k=1,b=0;//use equation y=kx+b to map value read from HX711 to weight.
int LF_Cnt=0;//Number of Referred Datapoints
double X[5],Y[5];//Referred Datapoints
inline double FUNC(double x){return k*x+b;}

//Below are for HX711
HX711 scale(7,6); // setup HX711
inline double Read_Value(){return scale.HX711_Read();}
inline double Read_Value(int k){
    double ret=0,maxx=-INF,minn=INF;
    for(int i=1;i<=k;++i){
      double tmp=Read_Value();ret+=tmp;maxx=max(maxx,tmp);minn=min(minn,tmp);
    }
    return (ret-maxx-minn)/(k-2);//exclude a max and a min, and return the mean value
}
inline double Get_Weight_g(double x=Read_Value()){return FUNC(x);}

//Below are for Print_Weight (Calculation)
const int sz=20;//set reference size=20
double reference[sz],sum_wei=0;//store the past few data and their sum
int refer_count=-1;//record the current position of newly added data
volatile bool refer_flag=0;//show whether reference has reached its size


//Below are for Print_Weight (Flag)
int Clock_Counter=0;// TimerFor STATUS 0
const int Timer_Period=20;// Set Timer Period = 20 ms


//Below are for Unit_Change
const int Unit_Num=4;
const String Units[Unit_Num]={"g","kg","oz","lb"};//They are all the possible units
const double rate[Unit_Num]={1,0.001,0.03527396,0.00220462};//g->Unit
int Unit_State=0;//The Index of Current Unit

//Flags for STATUS 0,3,4,5
volatile bool Print_Flag=0,Recalibration_Flag=0,Intricate_Calibration_Flag=0,Unit_Transfer_Flag=0;

//Below are for Print_Weight (Print)
void Print_Significant(double value,int Significant_Num=3){//setprecision(Significant_Num=3)
    //Exclude Cornercase
    if(value==0){
      Serial.print("0.00");
      return;
    }

    //Preparation work
    if(value<0)Serial.print("-"),value=-value;//Absolutize value in order to handle more easily
    bool xsd=0;//whether the float point is already printed

    //1.Make sure the highest position is not zero
    if(value<1){
      Serial.print("0.");xsd=1;
      value*=10;
      while(value<1){
        Serial.print("0");
        value*=10;
      }
    }

    //2. Calculate current exponent
    double v=value,multi=1;
    while(v>=10)v/=10,multi*=10;
    while(v<1)v*=10,multi/=10;

    //3. Round the value
    double Sig_rate=1;for(int i=1;i<=Significant_Num;++i)Sig_rate*=10;
    value+=5*multi/Sig_rate;

    //4. Get every Significant number
    while(Significant_Num--){
        if(multi<1 && !xsd)Serial.print("."),xsd=1;
        int now=value/multi;
        Serial.print(now);value-=multi*now;
        multi/=10;
    }

    //5. Print all the rest numbers(>0) as 0
    while(multi>=1){
      Serial.print("0");
      multi/=10;
    }
}

//Below is the logic of the two buttons
class Button{
private:
    volatile bool Long_Push_Timing,Button_Release_Flag;
    int Long_Push_Counter;
    int Position;
    int last_button;
    String ID;
public:
    inline bool Read(){
      return digitalRead(Position);
    }
    Button(int pos,String id){
        Long_Push_Timing=Button_Release_Flag=0;Long_Push_Counter=0;ID=id;
        Position=pos;pinMode(Position,INPUT_PULLUP);// enable the button
        last_button=Read();
    }
    inline bool Is_Timing(){
      return Long_Push_Timing;
    }
    inline int Push_Time(){//When visited, return ++counter
      return Timer_Period*(++Long_Push_Counter);
    }
    inline bool Is_Released(){
      return Button_Release_Flag;
    }
    inline void Release(){
      Button_Release_Flag=0;
    }
    void update(){
      bool now_button=Read();
      if(now_button && !last_button){//Release, Stop Timing;
        Button_Release_Flag=1;
        Long_Push_Timing=0;Long_Push_Counter=0;
      }
      else if(!now_button && last_button){// Push, Start Timing
        Button_Release_Flag=0;
        Long_Push_Timing=1;Long_Push_Counter=0;
      }
      //Serial.println(String(Button_Release_Flag)+String(Long_Push_Timing));
      last_button=now_button;
    }

    inline void Clear(){
      Long_Push_Timing=0,Button_Release_Flag=0;
      Clock_Counter=0,Long_Push_Counter=0;
      last_button=Read();//update initial state
    }
    void Detect_Push(bool output=1){
       if(output)// Optional Hint. Hide the hint(output=0) only when we need to actually flush the push state.
          Serial.println("Press "+ID+" to continue");//Default Mode: give client a hint to push the button
       while(!Is_Released());//Pause, until the button is released
       Release();//Reset this flag
    }
    inline double Calibrate_Step(String hint){//Hint, Print and Value Measure
      Serial.print(hint);
      Detect_Push();
      return Read_Value(10);
    }
};
Button Default_Button(PUSH1,"PUSH1"),Function_Button(PUSH2,"PUSH2");//P1.1 & P2.1


//STATUS 0: Print Current Weight
void Print_Weight(double x=Get_Weight_g()){
    Serial.print("WEIGHT: ");
    Print_Significant(x*rate[Unit_State]);
    Serial.println(" ("+Units[Unit_State]+")");
}

//STATUS 1: Welcome
void Welcome(){
    Print_Weight();
    Default_Button.Detect_Push();
}

//Below Are Preparation Functions for Calibration-Related Work.
void Modify_k(double _k){k=_k;}
void Modify_b(double _b){b=_b;}

void Add_Point(double x,double y){//Add datapoint
    Print_Weight(y);
    X[++LF_Cnt]=x;Y[LF_Cnt]=y;
}

void Linear_Fit(){//Set Linear Equation Parameters
    //least square method
    double sumX=0,sumY=0,sumXY=0,sumX2=0;
    for(int i=1;i<=LF_Cnt;i++){
      sumX+=X[i];sumY+=Y[i];
      sumXY+=X[i]*Y[i];sumX2+=X[i]*X[i];
    }
    Modify_k((LF_Cnt*sumXY-sumX*sumY)/(LF_Cnt*sumX2-sumX*sumX));
    Modify_b((sumY-k*sumX)/LF_Cnt);

    //Clear past datapoints
    memset(X,0,sizeof X);memset(Y,0,sizeof Y);LF_Cnt=0;
}
//Above Are Calibration Preparation Functions.

//STATUS 2: Scale_Calibration
void Scale_Calibration(){
    Serial.println("Start Scale Calibration.");
    Add_Point(Default_Button.Calibrate_Step("Empty the scale. "),0);
    Add_Point(Default_Button.Calibrate_Step("Put 50 gram in the scale. "),50);
    Linear_Fit();//Get k,b based on the two datapoints
    Serial.print("Scale is calibrated. ");
    Default_Button.Detect_Push();
}

//STATUS 3: Recalibration
void Recalibration(){
   Serial.println("Scale Recalibration Succeeded!");
   double mean_wei=sum_wei/(refer_flag?sz:(refer_count+1));
   Modify_b(-mean_wei+b);//Only need to modify b, don't need to change k
}

//STATUS 4: Scale_Intricate_Calibration
void Intricate_Calibration(){
    Serial.println("Start Intricate Calibration.");
    Function_Button.Detect_Push(0);//To eliminate the influence of long push
    Add_Point(Function_Button.Calibrate_Step("Empty the scale. "),0);
    Add_Point(Function_Button.Calibrate_Step("Put 10 gram in the scale. "),10);
    Add_Point(Function_Button.Calibrate_Step("Put 20 gram in the scale. "),20);
    Add_Point(Function_Button.Calibrate_Step("Put 50 gram in the scale. "),50);
    Linear_Fit();//Get k,b based on the above datapoints
    Serial.print("Scale is Intricately calibrated. ");Function_Button.Detect_Push();
}

//STATUS 5: Unit Transfer
void Unit_Transfer(){
  Serial.println("Unit Successfully Transferred to "+Units[(Unit_State+=1)%=Unit_Num]);//transfer unit to the next
}

//Timer
void isrTimer(void){
    if(Timer_Period*(++Clock_Counter)>=250)Print_Flag=1;//Loop Time Interval>=250ms ==> ENTER STATUS 0
    if(Function_Button.Is_Timing() && Function_Button.Push_Time()>=3000)Intricate_Calibration_Flag=1;//P2.1 Push Time>=3000ms ==> ENTER STATUS 4

    //update the button states
    Default_Button.update();
    Function_Button.update();
}

//setup
void setup() {
    //Preparation
    Serial.begin(9600);Serial.println("Welcome!");//switch Serial
    scale.begin();  //switch HX711
    SetTimer(isrTimer,20);//start timing

    //STATUS 1: Initial_Welcome
    Welcome();

    //STATUS 2: Scale_Calibration
    Scale_Calibration();
}

//Regular work: update weight per 20 millisec
double Weight_Update(){//with sliding window, update weight smoothly
    double ls_wei=0,mean_wei=0;if(refer_count>=0)ls_wei=reference[refer_count];
    if(++refer_count==sz)refer_count=0,refer_flag=1;
    double wei=Get_Weight_g(Read_Value()),pre_wei=reference[refer_count];
    if(abs(wei-ls_wei)>0.5){//unstable
      for(int i=0;i<sz;++i)reference[i]=0;
      refer_count=0,refer_flag=0;reference[0]=wei;sum_wei=wei;
      mean_wei=wei;
    }
    else{//stable
      reference[refer_count]=wei;sum_wei+=wei-pre_wei;
      mean_wei=sum_wei/(refer_flag?sz:(refer_count+1));
    }
    return mean_wei;
}

//Loop work
void loop() {
    //Regular work: update weight
    double mean_weight=Weight_Update();

    //STATUS 0: Print Weight
    if(Print_Flag){
      Print_Weight(mean_weight);
      Clock_Counter=0,Print_Flag=0;
    }

    //STATUS 3: Recalibration
    if(Recalibration_Flag=Default_Button.Is_Released()){
      Recalibration();
      Default_Button.Clear();Recalibration_Flag=0;
    }

    //STATUS 4: Intricate Calibration
    if(Intricate_Calibration_Flag){
      Intricate_Calibration();
      Function_Button.Clear();Intricate_Calibration_Flag=0;
    }

    //STATUS 5: Unit Transfer
    if(Unit_Transfer_Flag=Function_Button.Is_Released()){
      Unit_Transfer();
      Function_Button.Clear();Unit_Transfer_Flag=0;
    }
}
