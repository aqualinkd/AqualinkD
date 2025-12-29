
#include <stdio.h>
#include <stdlib.h>
#include <errno.h> 
#include <pthread.h>
//#include <errno.h>
#include <string.h>
#include <regex.h>
#include <fcntl.h>
#include <unistd.h>

#include "aqualink.h"
//#include "utils.h"
#include "sensors.h"


/*
----------
GPIO and thermal sensors are simply read value in file
----------
1Wire is similar to
/sys/bus/w1/devices/28-011564e2feff/w1_slave 
90 01 4b 46 7f ff 0c 10 33 : crc=33 YES
90 01 4b 46 7f ff 0c 10 33 t=25000
read the t= value.

regex would be something like .*t=([0-9|\.]*)
-----------
*/


/*
 read sensor value from ie /sys/class/thermal/thermal_zone0/temp

 return true if current reading is different to last value stored
 */

struct sensorthread {
  pthread_t thread_id;
  pthread_mutex_t thread_mutex;
  pthread_cond_t thread_cond;
  struct aqualinkdata *aqdata;
  struct timespec timeout;
};

struct sensorthread *_sthread = NULL;

void *sensors_worker( void *ptr );

void stop_sensors_thread() {
  LOG(AQUA_LOG, LOG_INFO, "Stopping sensor thread\n");

  if (_sthread != NULL)
    pthread_cond_broadcast(&_sthread->thread_cond);

}

void start_sensors_thread(struct aqualinkdata *aqdata) {

  _sthread = calloc(1, sizeof(struct sensorthread));

  _sthread->aqdata = aqdata;
  _sthread->thread_id = 0;

  if( pthread_create( &_sthread->thread_id , NULL ,  sensors_worker, (void*)_sthread) < 0) {
    LOG(AQUA_LOG, LOG_ERR, "could not create sensors thread\n");
    free(_sthread);
    return;
  }

  if ( _sthread->thread_id != 0 ) {
    pthread_detach(_sthread->thread_id);
  }
}

void *sensors_worker( void *ptr )
{
  struct sensorthread *sthread;
  sthread = (struct sensorthread *) ptr;
  int retval = 0;

  LOG(AQUA_LOG, LOG_NOTICE, "Started sensor thread\n");

  pthread_mutex_lock(&sthread->thread_mutex);

  do {
    if (retval != 0 && retval != ETIMEDOUT) {
       LOG(AQUA_LOG, LOG_ERR, "Sensor thread pthread_cond_timedwait failed for error %d %s\n",retval,strerror(retval));
      break;
    }

    for (int i=0; i < sthread->aqdata->num_sensors; i++) {
      //LOG(AQUA_LOG, LOG_DEBUG, "Sensor thread reading %s\n",sthread->aqdata->sensors[i].label);
      if (read_sensor(&sthread->aqdata->sensors[i]) ) {
        SET_DIRTY(sthread->aqdata->is_dirty);
      }
    }

    clock_gettime(CLOCK_REALTIME, &sthread->timeout);
    sthread->timeout.tv_sec += _aqconfig_.sensor_poll_time;
    //sthread->started_at = time(0);
    //LOG(AQUA_LOG, LOG_DEBUG, "Sensor thread will sleep for %d seconds\n",_aqconfig_.sensor_poll_time);
  } while ((retval = pthread_cond_timedwait(&sthread->thread_cond, &sthread->thread_mutex, &sthread->timeout)) == ETIMEDOUT);

  pthread_mutex_unlock(&sthread->thread_mutex);

  LOG(AQUA_LOG, LOG_DEBUG, "End sensor thread\n");

  free(sthread);
  pthread_exit(0);
}

#define READ_BUFFER_SIZE 256

bool read_sensor(external_sensor *sensor) {
  int fp;
  float value = 0.0;
  char buffer[READ_BUFFER_SIZE];
  char *startptr = &buffer[0];
  char *endptr;

  fp = open(sensor->path, O_RDONLY, 0);
  if (fp < 0) {
    LOGSystemError(errno, AQUA_LOG, sensor->path);
    LOG(AQUA_LOG,LOG_ERR, "Reading sensor %s %s\n",sensor->label, sensor->path);
    return FALSE;
  }

  // Read the sensor
  if ( read(fp, buffer, READ_BUFFER_SIZE) <= 0 ) {
    LOG(AQUA_LOG,LOG_ERR, "Reading value from sensor %s %s\n",sensor->label, sensor->path);
    close(fp);
    return FALSE;
  }
  close(fp);

  // If regex pass that
  if (sensor->regex != NULL) {
    regex_t preg;
    regmatch_t pmatch[2];
    //const char *pattern = ".*t=([0-9|\\.]*)";
    //const char *pattern = ".*([0-9]+).*";
    int status;
     // Compile the regular expression
    status = regcomp(&preg, sensor->regex, REG_EXTENDED);
    if (status != 0) {
      LOG(AQUA_LOG,LOG_ERR, "Compiling sensor regex %s\n",sensor->regex);
      return FALSE;
    }

    // Run regex
    if ( (status = regexec(&preg, buffer, 2, pmatch, 0)) == 0) {
        startptr = buffer + pmatch[1].rm_so;
    } else if (status == REG_NOMATCH) {
        //LOG(AQUA_LOG,LOG_DEBUG, "No sensor regex match '%s' on line '%s'\n",sensor->regex,line_buffer);
    } else {
        LOG(AQUA_LOG,LOG_ERR, "regex match error %d using '%s' on line '%s'\n",status,sensor->regex,buffer);
    }
    // Free the compiled regular expression
    regfree(&preg);
  }

  // Convert value to float
  value = strtof(startptr, &endptr);
  if (endptr == buffer) {
    LOG(AQUA_LOG,LOG_ERR, "Reading sensor value from %s\n", sensor->path);
  }

  value = value * sensor->factor;

  LOG(AQUA_LOG,LOG_DEBUG, "Read sensor %s value=%.2f\n",sensor->label, value);

  if (sensor->value != value) {
    sensor->value = value;
    return TRUE;
  }

  return FALSE;
}



#ifdef DO_NOT_COMPILE
/* TRY not to use this unless have too, unbuffered is far quicker for sysfs */
bool read_sensor_buffered(external_sensor *sensor) {

  FILE *fp;
  float value;

  fp = fopen(sensor->path, "r");
  if (fp == NULL) {
    LOGSystemError(errno, AQUA_LOG, sensor->path);
    LOG(AQUA_LOG,LOG_ERR, "Reading sensor %s %s\n",sensor->label, sensor->path);
    return FALSE;
  }

  if (sensor->regex != NULL) {
    char line_buffer[256];
    regex_t preg;
    regmatch_t pmatch[2];
    int status;
     // Compile the regular expression
    status = regcomp(&preg, sensor->regex, REG_EXTENDED);
    if (status != 0) {

      LOG(AQUA_LOG,LOG_ERR, "Compiling sensor regex %s\n",sensor->regex);
      return 1;
    }

    while (fgets(line_buffer, sizeof(line_buffer), fp) != NULL) {
      if ( (status = regexec(&preg, line_buffer, 2, pmatch, 0)) == 0) {
        value = atof(line_buffer + pmatch[1].rm_so);
        break;
      } else if (status == REG_NOMATCH) {
        //LOG(AQUA_LOG,LOG_DEBUG, "No sensor regex match '%s' on line '%s'\n",sensor->regex,line_buffer);
      } else {
        LOG(AQUA_LOG,LOG_ERR, "regex match error %d using '%s' on line '%s'\n",status,sensor->regex,line_buffer);
      }
    }
    // Free the compiled regular expression
    regfree(&preg);
  } else {
    fscanf(fp, "%f", &value);
  }

  fclose(fp);

  value = value * sensor->factor;
  //printf("Converted value %f\n",value);
  LOG(AQUA_LOG,LOG_DEBUG, "Read sensor %s value=%.2f\n",sensor->label, value);

  if (sensor->value != value) {
    sensor->value = value;
    return TRUE;
  }

  return FALSE;
}
#endif

