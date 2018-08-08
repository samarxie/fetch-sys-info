#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

#define CPU_NUM_PATH "/sys/devices/system/cpu/present"
#define SYSTEM_FPS_PATH "/proc/fps_tm/fps_count"
#define SYSTEM_GPU_FREQ_PATH "/sys/kernel/debug/ged/hal/current_freqency"
#define SYSTEM_GPU_LOADING_PATH "/sys/kernel/debug/ged/hal/gpu_utilization"
#define SYSTEM_GPU_AND_POWER_LIMIT_PATH "/proc/thermlmt"
#define SYSTEM_CPU_FREQ_PATH "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq"
#define SYSTEM_CPU_STAT "/proc/stat"
#define SYSTEM_INFO 10
#define CPU_TIME_NUM 7
typedef int int32;
static char outfile[10];
enum  eCsvWriteMode
{
    ECWM_ONELINE=1,
    ECWM_OTHERLINE,
};
static int cpu_tz_id = 0;
static int bat_tz_id = 0;
static int pcb_tz_id = 0;
static int sensor_temp;
static int cpu_num = 0;
static int thermal_zone_num(void);
static int fps_num = 0;
static int curr_gpu_freq = 0;
static int curr_gpu_loading = 0;
static int cpu_power_limit = 0;
static int gpu_power_limit = 0;
static unsigned long curr_cpu_freq[SYSTEM_INFO];
static unsigned long new_cpu_time[SYSTEM_INFO][CPU_TIME_NUM];
static unsigned long old_cpu_time[SYSTEM_INFO][CPU_TIME_NUM];
static unsigned long total_cpu_time[SYSTEM_INFO];
static unsigned long user_system_cpu_time[SYSTEM_INFO];
static unsigned long cpu_loading[SYSTEM_INFO];

#define ERR_RETURN(STR) printf(STR)

int32 putString2Csv(char str[],char filename[],int mode)
{
    FILE *_fp;
    //try to open filename
    if ((_fp=fopen(filename,"a"))==NULL) {
        ERR_RETURN("fopen called error");
    }
    int _mode=mode;
    switch(_mode) {
        case ECWM_ONELINE:
        {
            fputs(str,_fp);
            fputs("\t",_fp);
        }break;
        case ECWM_OTHERLINE:
        {
            fputs("\n",_fp);
        }break;
        default:
            break;
    }

    if (fclose(_fp) !=0){
        ERR_RETURN("fclose called error");
    }
    return 1;
}

/*
 * save file name:time
 * */
static char logfile[39];
static void create_collect_log_timestamp(void)
{
    struct tm *newtime;
    int len = 0;
    time_t t1;
    t1 = time(NULL);
    newtime=localtime(&t1);
    strftime( outfile, 10,"%H:%M:%S", newtime);
    outfile[10]=0;
}


static void create_save_log_file(void)
{
    struct tm *newtime;
    int len = 0;
    time_t t1;
    t1 = time(NULL);
    newtime = localtime(&t1);
    strftime(logfile,39,"thermal-log-%Y-%m-%d-%H:%M:%S.csv",newtime);
    logfile[38] = 0;
}

/*
* get cpu battery and pcb temp
* */

static int get_sensor_temp(int sensor_id)
{
    char temp[50];
    FILE *fp;
    char buf[5];
    sprintf(temp,"/sys/class/thermal/thermal_zone%d/temp",sensor_id);
    if((fp = fopen(temp,"r")) != NULL) {
        fscanf(fp,"%d",&sensor_temp);
        fclose(fp);
    }
//    strcpy(sensor_temp,buf);
    return sensor_temp;
}

static int get_cpu_num(void)
{
    char temp[100];
    FILE *fp;
    char buf[4];
    if((fp = fopen(CPU_NUM_PATH,"r")) != NULL) {
        fread(buf,1,4,fp);
        buf[4]=0;
    }
    cpu_num = atoi(buf + 2);
    return cpu_num;
}
/*
 *
 * get cpu freq
 *
 * */
static unsigned long get_cpu_freq()
{
    unsigned long a[10];
    int i = 0;
    FILE *fp;
    for(; i < cpu_num + 1; i++) {
        char temp[100];
        sprintf(temp,SYSTEM_CPU_FREQ_PATH,i);
        if((fp = fopen(temp,"r")) !=NULL) {
            fscanf(fp,"%lu",&curr_cpu_freq[i]);
            fclose(fp);
        } else {
            curr_cpu_freq[i] = 0;
        }
    }
    return 0 ;
}

/*
* the function can optimization
* */
static int get_cpu_loading()
{
    FILE *fp = fopen(SYSTEM_CPU_STAT, "r");
    char *delim = " ";
    char szTest[1000] = {0};
    char *p;
    char cpu[5];
    int i = 0;
    int cpu_id = 0;

    if(NULL == fp) {
        printf("failed to open %s file node\n",SYSTEM_CPU_STAT);
        return 1;
    }

    while(!feof(fp)) {
        memset(szTest, 0, sizeof(szTest));
        fgets(szTest, sizeof(szTest) - 1, fp);//include<\n>
        if(szTest[3] == ' ')
            continue;

        if(szTest[0] != 'c')
            break;

        cpu_id = szTest[3] - '0';

        sscanf(szTest,"%s %lu %lu %lu %lu %lu %lu %lu",
                            cpu,&new_cpu_time[cpu_id][0],&new_cpu_time[cpu_id][1],
                            &new_cpu_time[cpu_id][2],&new_cpu_time[cpu_id][3],
                            &new_cpu_time[cpu_id][4],&new_cpu_time[cpu_id][5],
                            &new_cpu_time[cpu_id][6]);
    }
    fclose(fp);
    return 0;
}

/*
* get thermal_zone ID
*/
static int all_thermal_zone_num = 0;
static int thermal_zone_num(void)
{
    int i = 0;
    char temp[50];
    char buf[15];
    FILE *fp;
    while(1) {
        sprintf(temp,"/sys/class/thermal/thermal_zone%d/type",i);
        if((fp = fopen(temp,"r")) !=NULL) {
            all_thermal_zone_num++;
            fread(buf,1,15,fp);
            buf[14]=0;
            if(strncmp(buf,"mtktsAP",6/*"mtktsbattery",12*/) == 0)
                pcb_tz_id = i;
            else if(strncmp(buf,"mtktscpu",8/*"mtktscpu",8*/) == 0)
                cpu_tz_id = i;
            else if(strncmp(buf,"mtktsbattery",12) == 0)
                bat_tz_id = i;
            fclose(fp);
        } else {
			break；
		}
        i++;
    }
    return all_thermal_zone_num;
}

/*
* get fps
* */
static int get_system_fps(void)
{
    FILE *fp;
    if((fp = fopen(SYSTEM_FPS_PATH,"r")) != NULL) {
        fscanf(fp,"%d",&fps_num);
        fclose(fp);
    }
    return fps_num;
}


static int get_system_gpu_and_power_limit(void)
{
    FILE *fp;
    if((fp = fopen(SYSTEM_GPU_AND_POWER_LIMIT_PATH,"r")) != NULL) {
        fscanf(fp,"%d,%d,%d,%d",&cpu_power_limit,&gpu_power_limit,&curr_gpu_loading,&curr_gpu_freq);
        fclose(fp);
    }
    return 0 ;
}

static void cacl_cpu_loading()
{
    int i = 0,j = 0;
    for(i = 0; i < SYSTEM_INFO; i++) {
        if(new_cpu_time[i][0] == old_cpu_time[i][0]) {
            cpu_loading[i] = 0;
            continue;
        }
        for( j = 0; j < CPU_TIME_NUM; j++ ) {
            total_cpu_time[i] += (new_cpu_time[i][j] - old_cpu_time[i][j]);
        }
        user_system_cpu_time[i] = (new_cpu_time[i][0] + new_cpu_time[i][2]) - (old_cpu_time[i][0] + old_cpu_time[i][2]);
        cpu_loading[i] = user_system_cpu_time[i] * 100 / total_cpu_time[i];
    }
}

static const char *title_name[29]={"time","cpu_temp","bat_temp","pcb_temp","cpu0_freq",
                                  "cpu0_load","cpu1_freq","cpu1_load","cpu2_freq","cpu2_load",
                                  "cpu3_freq","cpu3_load","cpu4_freq","cpu4_load","cpu5_freq",
                                  "cpu5_load","cpu6_freq","cpu6_load","cpu7_freq","cpu7_load",
                                  "cpu8_freq","cpu8_load","cpu9_freq","cpu9_load","gpu_freq",
                                   "gpu_load","fps","cpu_limit","gpu_limit"};

static void usage(char *cmd)
{
    fprintf(stderr, "Usage: %s [ -d delay_time ] [ -r running_time ] [ -h ]\n"
                    "    -d num  sleep num collect a thermal data, unit is seconds.\n"
                    "    -r num  Collect num thermal data.\n"
                    "    -h      Display this help screen.\n"
                    "    default running a hour time per a second to collect thermal data.\n",
        cmd);
}

int le_thermal_main(int argc, char *argv[])
{
    int i = 0,j = 0;
    unsigned long time = 0;
	int delay_time = 1;
	unsigned long running_time = 3600;
    create_save_log_file();
    char save_log_path[55] = "/storage/sdcard0/";
    strcat(save_log_path,logfile);
    printf("save log path is: %s\n",save_log_path);
    for(i = 1; i < argc; i++) {
	   if(!strcmp(argv[i],"-d")) {
		   if(i + 1 >= argc) {
			   fprintf(stderr,"Option -d expects an argument\n");
			   usage(argv[0]);
			   return 0;
		   }
		   delay_time = atoi(argv[++i]);
           if(delay_time > 3)
                delay_time = 3;
	   }
	   if(!strcmp(argv[i],"-r")) {
		   if(i + 1 >= argc) {
			   fprintf(stderr,"Option -r expects an argument\n");
			   usage(argv[0]);
		   }
		   running_time = atol(argv[++i]);
		   continue;
	   }
	   if(!strcmp(argv[i],"-h")) {
		   usage(argv[0]);
		   return 0;
	   }
	   fprintf(stderr, "Invalid argument \"%s\".\n",argv[i]);
	   usage(argv[0]);
	   return 0;
    }

    for (i = 0;i < 29; ++i) {
        char strtemp[256];
        sprintf(strtemp,"%s",title_name[i]);
        putString2Csv(strtemp,save_log_path,ECWM_ONELINE);
    }
    putString2Csv("",save_log_path,ECWM_OTHERLINE);//must

    get_cpu_num();
    get_cpu_loading();
    while (time++ < running_time) {
        /*save old cpu idle and busy time*/
        for( i = 0;i < SYSTEM_INFO; i++ ) {
            for( j = 0;j < CPU_TIME_NUM;j++) {
                old_cpu_time[i][j] = new_cpu_time[i][j];
            }
        }

        sleep(delay_time);

        for( i = 0; i< SYSTEM_INFO;i++ ) {
            total_cpu_time[i] = 0;
            user_system_cpu_time[i] = 0;
        }

        get_cpu_loading();
        cacl_cpu_loading();
        /*
        * collect data and format data.
        * */
            thermal_zone_num();
            get_system_fps();
            get_cpu_freq();
            get_system_gpu_and_power_limit();
            char strtemp[256];
            create_collect_log_timestamp();
            sprintf(strtemp,"%s",outfile);
            putString2Csv(strtemp,save_log_path,ECWM_ONELINE);

            sprintf(strtemp,"%d",get_sensor_temp(cpu_tz_id));
            putString2Csv(strtemp,save_log_path,ECWM_ONELINE);
            sprintf(strtemp,"%d",get_sensor_temp(bat_tz_id));
            putString2Csv(strtemp,save_log_path,ECWM_ONELINE);
            sprintf(strtemp,"%d",get_sensor_temp(pcb_tz_id));
            putString2Csv(strtemp,save_log_path,ECWM_ONELINE);

            for(i = 0;i < SYSTEM_INFO;i++) {
                sprintf(strtemp,"%lu",curr_cpu_freq[i]);
                putString2Csv(strtemp,save_log_path,ECWM_ONELINE);
                sprintf(strtemp,"%lu",cpu_loading[i]);
                putString2Csv(strtemp,save_log_path,ECWM_ONELINE);
            }


            sprintf(strtemp,"%d",curr_gpu_freq);
            putString2Csv(strtemp,save_log_path,ECWM_ONELINE);

            sprintf(strtemp,"%d",curr_gpu_loading);
            putString2Csv(strtemp,save_log_path,ECWM_ONELINE);
            sprintf(strtemp,"%d",fps_num);
            putString2Csv(strtemp,save_log_path,ECWM_ONELINE);
            sprintf(strtemp,"%d",cpu_power_limit);
            putString2Csv(strtemp,save_log_path,ECWM_ONELINE);

            sprintf(strtemp,"%d",gpu_power_limit);
            putString2Csv(strtemp,save_log_path,ECWM_ONELINE);

            putString2Csv("",save_log_path,ECWM_OTHERLINE);//must
#if 1
        for( i = 0;i < SYSTEM_INFO;i++ ) {
            for(j = 0;j < CPU_TIME_NUM;j++) {
                old_cpu_time[i][j] = 0;
            }
            cpu_loading[i] = 0;
        }
#endif
    }
    return 0;
}
