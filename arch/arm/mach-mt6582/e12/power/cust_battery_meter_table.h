#ifndef _CUST_BATTERY_METER_TABLE_H
#define _CUST_BATTERY_METER_TABLE_H

#include <mach/mt_typedefs.h>

// ============================================================
// define
// ============================================================
#define BAT_NTC_10 1
#define BAT_NTC_47 0

#if (BAT_NTC_10 == 1)
#define RBAT_PULL_UP_R             16900	
#define RBAT_PULL_DOWN_R		   30000 //20140527, JimCheng Modify for HW Design 	
#endif

#if (BAT_NTC_47 == 1)
#define RBAT_PULL_UP_R             61900	
#define RBAT_PULL_DOWN_R		  100000	
#endif
#define RBAT_PULL_UP_VOLT          1800



// ============================================================
// ENUM
// ============================================================

// ============================================================
// structure
// ============================================================

// ============================================================
// typedef
// ============================================================
typedef struct _BATTERY_PROFILE_STRUC
{
    kal_int32 percentage;
    kal_int32 voltage;
} BATTERY_PROFILE_STRUC, *BATTERY_PROFILE_STRUC_P;

typedef struct _R_PROFILE_STRUC
{
    kal_int32 resistance; // Ohm
    kal_int32 voltage;
} R_PROFILE_STRUC, *R_PROFILE_STRUC_P;

typedef enum
{
    T1_0C,
    T2_25C,
    T3_50C
} PROFILE_TEMPERATURE;

// ============================================================
// External Variables
// ============================================================

// ============================================================
// External function
// ============================================================

// ============================================================
// <DOD, Battery_Voltage> Table
// ============================================================
#if (BAT_NTC_10 == 1)
    BATT_TEMPERATURE Batt_Temperature_Table[] = {
/* [Arima JimCheng 20140527] JimCheng Modify for Battery Parameter ++*/ 
        {-20,317500},
        {0,108100},
        {5,84100},
        { 10,65900},
        {  15,52000},
        {  20,41300},
        { 25,33000},
        { 30,26500},
        { 35,21400},
        { 40,17400},
        { 45,14300},
        { 50,11700},
        { 55,9700},
        { 60,8000},
        { 65,6700},
        { 70,5600},
        { 100,2200}
/* [Arima JimCheng 20140527] JimCheng Modify for Battery Parameter --*/
    };
#endif

#if (BAT_NTC_47 == 1)
    BATT_TEMPERATURE Batt_Temperature_Table[] = {
        {-20,483954},
        {-15,360850},
        {-10,271697},
        { -5,206463},
        {  0,158214},
        {  5,122259},
        { 10,95227},
        { 15,74730},
        { 20,59065},
        { 25,47000},
        { 30,37643},
        { 35,30334},
        { 40,24591},
        { 45,20048},
        { 50,16433},
        { 55,13539},
        { 60,11210}        
    };
#endif

/* [Arima JimCheng 20141204]  add for charging feature ++ */
// T0 -10C
BATTERY_PROFILE_STRUC battery_profile_t0[] =
{
	{0    , 4312},         
	{1    , 4282},         
	{3    , 4246},         
	{4    , 4222},         
	{5    , 4203},         
	{7    , 4187},         
	{8    , 4171},         
	{10  , 4156},         
	{11  , 4143},         
	{12  , 4129},         
	{14  , 4116},         
	{15  , 4102},         
	{17  , 4089},         
	{18  , 4077},         
	{19  , 4064},         
	{21  , 4052},         
	{22  , 4039},         
	{23  , 4026},         
	{25  , 4015},         
	{26  , 4003},         
	{28  , 3991},         
	{29  , 3979},         
	{30  , 3967},         
	{32  , 3956},         
	{33  , 3944},         
	{34  , 3932},         
	{36  , 3921},         
	{37  , 3911},         
	{39  , 3900},         
	{40  , 3890},         
	{41  , 3880},         
	{43  , 3872},         
	{44  , 3864},         
	{46  , 3856},         
	{47  , 3849},         
	{48  , 3842},         
	{50  , 3835},         
	{51  , 3829},         
	{52  , 3824},         
	{54  , 3818},         
	{55  , 3812},         
	{57  , 3807},         
	{58  , 3802},         
	{59  , 3798},         
	{61  , 3794},         
	{62  , 3790},         
	{63  , 3786},         
	{65  , 3783},         
	{66  , 3779},         
	{68  , 3776},         
	{69  , 3772},         
	{70  , 3770},         
	{72  , 3767},         
	{73  , 3763},         
	{74  , 3759},         
	{76  , 3755},         
	{77  , 3751},         
	{79  , 3745},         
	{80  , 3740},         
	{81  , 3733},         
	{83  , 3727},          
	{84  , 3719},         
	{85  , 3711},         
	{87  , 3701},         
	{88  , 3690},         
	{90  , 3682},         
	{91  , 3674},         
	{92  , 3666},         
	{94  , 3659},         
	{95  , 3645},         
	{97  , 3615},         
	{98  , 3575},         
	{98  , 3549},         
	{99  , 3530},  
	{99  , 3515},         
	{99  , 3503},         
	{99  , 3493},         
	{99  , 3484},         
	{100  , 3479},       
	{100  , 3473},              
	{100  , 3468},       
	{100  , 3464},       
	{100  , 3460},       
	{100  , 3400}
};      
        
// T1 0C 
BATTERY_PROFILE_STRUC battery_profile_t1[] =
{
	{0    , 4313},         
	{1    , 4286},         
	{3    , 4267},         
	{4    , 4249},         
	{5    , 4234},         
	{7    , 4218},         
	{8    , 4205},         
	{9    , 4189},         
	{11  , 4176},         
	{12  , 4162},         
	{13  , 4148},         
	{15  , 4135},         
	{16  , 4122},         
	{17  , 4109},         
	{18  , 4096},         
	{20  , 4084},         
	{21  , 4072},         
	{22  , 4060},         
	{24  , 4048},         
	{25  , 4036},         
	{26  , 4024},         
	{28  , 4013},         
	{29  , 4002},         
	{30  , 3990},         
	{32  , 3979},         
	{33  , 3968},         
	{34  , 3957},         
	{36  , 3944},         
	{37  , 3933},         
	{38  , 3922},         
	{40  , 3910},         
	{41  , 3900},         
	{42  , 3890},         
	{44  , 3881},         
	{45  , 3873},         
	{46  , 3864},         
	{48  , 3857},         
	{49  , 3850},         
	{50  , 3843},         
	{52  , 3836},         
	{53  , 3830},         
	{54  , 3826},         
	{55  , 3820},         
	{57  , 3816},         
	{58  , 3810},         
	{59  , 3806},         
	{61  , 3802},         
	{62  , 3797},         
	{63  , 3794},         
	{65  , 3790},         
	{66  , 3786},         
	{67  , 3783},         
	{69  , 3780},         
	{70  , 3776},         
	{71  , 3773},         
	{73  , 3770},         
	{74  , 3766},         
	{75  , 3762},         
	{77  , 3757},         
	{78  , 3753},         
	{79  , 3748},          
	{81  , 3743},         
	{82  , 3737},         
	{83  , 3730},         
	{85  , 3722},         
	{86  , 3716},         
	{87  , 3708},         
	{88  , 3696},         
	{90  , 3688},         
	{91  , 3684},         
	{92  , 3679},         
	{94  , 3673},         
	{95  , 3661},         
	{96  , 3624},  
	{98  , 3562},         
	{99  , 3478},         
	{100  , 3409},         
	{100  , 3381},         
	{100  , 3366},       
	{100  , 3356},              
	{100  , 3349},       
	{100  , 3345},       
	{100  , 3341},       
	{100  , 3338}
};           

// T2 25C
BATTERY_PROFILE_STRUC battery_profile_t2[] =
{
	{0    , 4332},         
	{1    , 4311},         
	{3    , 4293},         
	{4    , 4277},         
	{5    , 4262},         
	{6    , 4247},         
	{8    , 4233},         
	{9    , 4218},         
	{10  , 4205},         
	{11  , 4191},         
	{13  , 4177},         
	{14  , 4163},         
	{15  , 4151},         
	{17  , 4138},         
	{18  , 4125},         
	{19  , 4112},         
	{20  , 4100},         
	{22  , 4087},         
	{23  , 4075},         
	{24  , 4063},         
	{25  , 4052},         
	{27  , 4041},         
	{28  , 4029},         
	{29  , 4019},         
	{30  , 4008},         
	{32  , 3997},         
	{33  , 3987},         
	{34  , 3977},         
	{36  , 3966},         
	{37  , 3955},         
	{38  , 3945},         
	{39  , 3933},         
	{41  , 3921},         
	{42  , 3908},         
	{43  , 3898},         
	{45  , 3887},         
	{46  , 3878},         
	{47  , 3869},         
	{48  , 3862},         
	{50  , 3854},         
	{51  , 3847},         
	{52  , 3842},         
	{53  , 3836},         
	{55  , 3830},         
	{56  , 3826},         
	{57  , 3820},         
	{58  , 3816},         
	{60  , 3811},         
	{61  , 3807},         
	{62  , 3803},         
	{64  , 3799},         
	{65  , 3795},         
	{66  , 3791},         
	{67  , 3788},         
	{69  , 3784},         
	{70  , 3780},         
	{71  , 3776},         
	{72  , 3771},         
	{74  , 3766},         
	{75  , 3760},         
	{76  , 3754},          
	{78  , 3748},         
	{79  , 3745},         
	{80  , 3741},         
	{81  , 3736},         
	{83  , 3730},         
	{84  , 3723},         
	{85  , 3716},         
	{86  , 3710},         
	{88  , 3701},         
	{89  , 3691},         
	{90  , 3686},         
	{92  , 3683},         
	{93  , 3680},  
	{94  , 3674},         
	{95  , 3660},         
	{97  , 3621},         
	{98  , 3562},         
	{99  , 3482},       
	{100  , 3359},              
	{100  , 3285},       
	{100  , 3273},       
	{100  , 3267},       
	{100  , 3266}
};     

// T3 50C
BATTERY_PROFILE_STRUC battery_profile_t3[] =
{
	{0    , 4339},         
	{1    , 4320},         
	{3    , 4302},         
	{4    , 4285},         
	{5    , 4270},         
	{6    , 4255},         
	{8    , 4240},         
	{9    , 4226},         
	{10  , 4212},         
	{11  , 4199},         
	{13  , 4184},         
	{14  , 4171},         
	{15  , 4157},         
	{17  , 4144},         
	{18  , 4132},         
	{19  , 4119},         
	{20  , 4106},         
	{22  , 4093},         
	{23  , 4081},         
	{24  , 4069},         
	{25  , 4057},         
	{27  , 4046},         
	{28  , 4035},         
	{29  , 4024},         
	{30  , 4014},         
	{32  , 4002},         
	{33  , 3992},         
	{34  , 3982},         
	{36  , 3972},         
	{37  , 3962},         
	{38  , 3952},         
	{39  , 3942},         
	{41  , 3930},         
	{42  , 3918},         
	{43  , 3904},         
	{44  , 3893},         
	{46  , 3883},         
	{47  , 3874},         
	{48  , 3865},         
	{50  , 3859},         
	{51  , 3851},         
	{52  , 3844},         
	{53  , 3838},         
	{55  , 3832},         
	{56  , 3827},         
	{57  , 3822},         
	{58  , 3818},         
	{60  , 3812},         
	{61  , 3807},         
	{62  , 3803},         
	{63  , 3799},         
	{65  , 3795},         
	{66  , 3791},         
	{67  , 3788},         
	{69  , 3784},         
	{70  , 3779},         
	{71  , 3771},         
	{72  , 3761},         
	{74  , 3753},         
	{75  , 3746},         
	{76  , 3741},          
	{77  , 3735},         
	{79  , 3729},         
	{80  , 3726},         
	{81  , 3721},         
	{83  , 3715},         
	{84  , 3709},         
	{85  , 3701},         
	{86  , 3695},         
	{88  , 3688},         
	{89  , 3677},         
	{90  , 3672},         
	{91  , 3670},         
	{93  , 3667},  
	{94  , 3662},         
	{95  , 3649},         
	{96  , 3611},         
	{98  , 3555},         
	{99  , 3481},       
	{100  , 3374},              
	{100  , 3269},       
	{100  , 3256},       
	{100  , 3253},       
	{100  , 3252}
};    
/* [Arima JimCheng 20141204]  add for charging feature -- */

// battery profile for actual temperature. The size should be the same as T1, T2 and T3
BATTERY_PROFILE_STRUC battery_profile_temperature[] =
{
// [Arima JimCheng 20140528] Default setting by MTK suggestion
  {0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },  
	{0  , 0 }, 
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },  
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 }
};    

// ============================================================
// <Rbat, Battery_Voltage> Table
// ============================================================
// T0 -10C
R_PROFILE_STRUC r_profile_t0[] =
{
	{360  , 4161},         
	{498  , 4139},         
	{523  , 4113},         
	{553  , 4080},         
	{623  , 4037},         
	{785  , 4005},         
	{818  , 3986},         
	{835  , 3971},         
	{838  , 3956},         
	{845  , 3943},         
	{845  , 3928},         
	{848  , 3915},         
	{845  , 3898},         
	{838  , 3884},         
	{838  , 3869},         
	{840  , 3857},         
	{843  , 3846},         
	{843  , 3835},         
	{850  , 3826},         
	{863  , 3819},         
	{873  , 3813},         
	{883  , 3806},         
	{893  , 3800},         
	{905  , 3795},         
	{915  , 3792},         
	{928  , 3788},         
	{930  , 3783},         
	{948  , 3782},         
	{960  , 3778},         
	{968  , 3772},         
	{985  , 3768},         
	{998  , 3761},         
	{1013 , 3752},         
	{1025 , 3740},         
	{1050 , 3729},         
	{1080 , 3717},         
	{1128 , 3708},         
	{1208 , 3701},         
	{1240 , 3695},         
	{1228 , 3690},         
	{1220 , 3687},         
	{1213 , 3683},         
	{1205 , 3681},         
	{1195 , 3677},         
	{1185 , 3672},         
	{1173 , 3668},         
	{1160 , 3661},         
	{1150 , 3658},         
	{1135 , 3653},         
	{1125 , 3649},         
	{1113 , 3642},         
	{1103 , 3638},         
	{1085 , 3633},         
	{1080 , 3629},         
	{1073 , 3625},         
	{1060 , 3621},         
	{1055 , 3619},         
	{1045 , 3614},         
	{1043 , 3612},         
	{1030 , 3611},         
	{1030 , 3608},          
  {1028 , 3606},
  {1013 , 3605},
  {1015 , 3603},
  {1008 , 3601},
  {1003 , 3599},
  {1008 , 3598},
  {998  , 3595},
  {1000 , 3593},
  {1003 , 3592},
	{1005 , 3590}, 
	{980  , 3588},
	{980  , 3586},
	{993  , 3586},
	{983  , 3585},
	{983  , 3584}, 
	{983  , 3582}	       
};      

// T1 0C
R_PROFILE_STRUC r_profile_t1[] =
{
	{268  , 4160},         
	{388  , 4109},         
	{468  , 4080},         
	{483  , 4060},         
	{495  , 4041},         
	{498  , 4022},         
	{513  , 4006},         
	{518  , 3992},         
	{523  , 3978},         
	{523  , 3965},         
	{535  , 3954},         
	{535  , 3940},         
	{533  , 3928},         
	{533  , 3915},         
	{525  , 3900},         
	{518  , 3884},         
	{508  , 3868},         
	{500  , 3855},         
	{495  , 3842},         
	{495  , 3833},         
	{495  , 3824},         
	{493  , 3816},         
	{500  , 3811},         
	{508  , 3804},         
	{515  , 3800},         
	{518  , 3794},         
	{523  , 3789},         
	{533  , 3787},         
	{543  , 3785},         
	{543  , 3781},         
	{545  , 3779},         
	{555  , 3778},         
	{555  , 3774},         
	{565  , 3771},         
	{570  , 3765},         
	{580  , 3759},         
	{580  , 3748},         
	{595  , 3739},         
	{603  , 3727},         
	{608  , 3712},         
	{630  , 3701},         
	{673  , 3694},         
	{750  , 3685},         
	{890  , 3677},         
	{1123 , 3654},         
	{1073 , 3628},         
	{1020 , 3607},         
	{978  , 3588},         
	{935  , 3572},         
	{908  , 3560},         
	{885  , 3550},         
	{850  , 3540},         
	{833  , 3532},         
	{828  , 3527},         
	{808  , 3520},         
	{790  , 3513},         
	{785  , 3509},         
	{768  , 3505},         
	{755  , 3501},         
	{763  , 3498},         
	{750  , 3495},          
  {748  , 3491},
  {725  , 3488},
  {728  , 3487},
  {730  , 3483},
  {705  , 3482},
  {705  , 3479},
  {725  , 3479},
  {713  , 3476},
  {715  , 3475},
	{715  , 3473}, 
	{718  , 3472},
	{718  , 3468},
	{718  , 3467},
	{695  , 3466},
	{668  , 3463}, 
	{690  , 3463}	       
};     

// T2 25C
R_PROFILE_STRUC r_profile_t2[] =
{
	{150  , 4179},         
	{205  , 4155},         
	{210  , 4136},         
	{215  , 4118},         
	{215  , 4098},         
	{223  , 4082},         
	{228  , 4065},         
	{230  , 4048},         
	{230  , 4029},         
	{238  , 4014},         
	{240  , 4000},         
	{245  , 3986},         
	{250  , 3973},         
	{250  , 3959},         
	{253  , 3946},         
	{265  , 3936},         
	{268  , 3924},         
	{270  , 3914},         
	{270  , 3901},         
	{268  , 3889},         
	{240  , 3867},         
	{223  , 3850},         
	{218  , 3837},         
	{213  , 3827},         
	{213  , 3819},         
	{215  , 3812},         
	{220  , 3806},         
	{223  , 3800},         
	{225  , 3796},         
	{228  , 3791},         
	{233  , 3788},         
	{223  , 3781},         
	{230  , 3778},         
	{233  , 3777},         
	{238  , 3774},         
	{238  , 3771},         
	{238  , 3769},         
	{230  , 3762},         
	{228  , 3755},         
	{220  , 3746},         
	{225  , 3739},         
	{230  , 3730},         
	{230  , 3718},         
	{233  , 3704},         
	{223  , 3688},         
	{228  , 3681},         
	{243  , 3677},         
	{263  , 3670},         
	{283  , 3644},         
	{315  , 3562},         
	{403  , 3408},         
	{380  , 3349},         
	{330  , 3329},         
	{305  , 3319},         
	{288  , 3313},         
	{275  , 3309},         
	{273  , 3304},         
	{260  , 3302},         
	{260  , 3300},         
	{248  , 3296},         
	{243  , 3295},          
  {250  , 3297},
  {248  , 3296},
  {243  , 3296},
  {243  , 3296},
  {235  , 3293},
  {223  , 3289},
  {233  , 3289},
  {228  , 3286},
  {223  , 3285},
	{225  , 3284}, 
	{225  , 3284},
	{218  , 3284},
	{220  , 3284},
	{218  , 3283},
	{220  , 3282}, 
	{213  , 3281}	       
}; 

// T3 50C
R_PROFILE_STRUC r_profile_t3[] =
{
	{150  , 4179},         
	{205  , 4155},         
	{210  , 4136},         
	{215  , 4118},         
	{215  , 4098},         
	{223  , 4082},         
	{228  , 4065},         
	{230  , 4048},         
	{230  , 4029},         
	{238  , 4014},         
	{240  , 4000},         
	{245  , 3986},         
	{250  , 3973},         
	{250  , 3959},         
	{253  , 3946},         
	{265  , 3936},         
	{268  , 3924},         
	{270  , 3914},         
	{270  , 3901},         
	{268  , 3889},         
	{240  , 3867},         
	{223  , 3850},         
	{218  , 3837},         
	{213  , 3827},         
	{213  , 3819},         
	{215  , 3812},         
	{220  , 3806},         
	{223  , 3800},         
	{225  , 3796},         
	{228  , 3791},         
	{233  , 3788},         
	{223  , 3781},         
	{230  , 3778},         
	{233  , 3777},         
	{238  , 3774},         
	{238  , 3771},         
	{238  , 3769},         
	{230  , 3762},         
	{228  , 3755},         
	{220  , 3746},         
	{225  , 3739},         
	{230  , 3730},         
	{230  , 3718},         
	{233  , 3704},         
	{223  , 3688},         
	{228  , 3681},         
	{243  , 3677},         
	{263  , 3670},         
	{283  , 3644},         
	{315  , 3562},         
	{403  , 3408},         
	{380  , 3349},         
	{330  , 3329},         
	{305  , 3319},         
	{288  , 3313},         
	{275  , 3309},         
	{273  , 3304},         
	{260  , 3302},         
	{260  , 3300},         
	{248  , 3296},         
	{243  , 3295},          
  {250  , 3297},
  {248  , 3296},
  {243  , 3296},
  {243  , 3296},
  {235  , 3293},
  {223  , 3289},
  {233  , 3289},
  {228  , 3286},
  {223  , 3285},
	{225  , 3284}, 
	{225  , 3284},
	{218  , 3284},
	{220  , 3284},
	{218  , 3283},
	{220  , 3282}, 
	{213  , 3281}	       
}; 

// r-table profile for actual temperature. The size should be the same as T1, T2 and T3
R_PROFILE_STRUC r_profile_temperature[] =
{
  {0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },  
	{0  , 0 }, 
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },  
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 }
};    

// ============================================================
// function prototype
// ============================================================
int fgauge_get_saddles(void);
BATTERY_PROFILE_STRUC_P fgauge_get_profile(kal_uint32 temperature);

int fgauge_get_saddles_r_table(void);
R_PROFILE_STRUC_P fgauge_get_profile_r_table(kal_uint32 temperature);

#endif	//#ifndef _CUST_BATTERY_METER_TABLE_H

