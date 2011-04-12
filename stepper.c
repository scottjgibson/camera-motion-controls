#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <ftdi.h>

// FTDI board | Stepper Driver
// D7 RI  |
// D6 DCD | MS3
// D5 DSR | MS2
// D4 DTR | MS1
// D3 CTS | shutter
// D2 RTS | dir
// D1 RXD | step
// D0 TXD | ~enable

int init_ftdi(struct ftdi_context *ftdic)
{
	int rc;

    if (ftdi_init(ftdic) < 0)
    {
        fprintf(stderr, "ftdi_init failed\n");
        return -1;
    }

    rc = ftdi_usb_open(ftdic, 0x0403, 0x6001);

    if (rc < 0 && rc != -5)
    {
        fprintf(stderr, "unable to open ftdi device: %d (%s)\n", rc, ftdi_get_error_string(ftdic));
        return -1;
    }

    ftdi_set_bitmode(ftdic, 0xFF, BITMODE_BITBANG);
	return 0;
}

#define FULL_STEP 0x00
#define HALF_STEP 0x10
#define QUARTER_STEP 0x20
#define EIGHTH_STEP 0x30
#define SIXTEENTH_STEP 0x70

#define _ENA_BIT 0x01
#define DIR_BIT 0x04
#define STEP_BIT 0x02
#define SHUTTER_BIT 0x08


struct stepper_context {
	uint32_t travelPerShot;
	uint32_t mmPerFullStep;
	uint8_t  stepsPerFullStep; //1 = Full step; 2 = half step ..
	uint32_t delayPerStepSlow;
	uint32_t delayPerStepFast;
	uint32_t settleTimePerShot;
	uint8_t regVal;
};

struct stepper_context stepperCtx;


void stepperConfig( uint32_t mmPerFullStep, 
				    uint8_t stepsPerFullStep, 
				    uint32_t delayPerStepSlow, 
				    uint32_t delayPerStepFast,
			    	uint32_t settleTimePerShot)
{
	stepperCtx.mmPerFullStep = mmPerFullStep;
	stepperCtx.stepsPerFullStep = stepsPerFullStep;
	stepperCtx.delayPerStepSlow = delayPerStepSlow;
	stepperCtx.delayPerStepFast = delayPerStepFast;
	stepperCtx.settleTimePerShot = settleTimePerShot;
	stepperCtx.regVal = 0x01;
	switch(stepsPerFullStep)
	{
		case 2:
		stepperCtx.regVal |= HALF_STEP;
		break;

		case 4:
		stepperCtx.regVal |= QUARTER_STEP;
		break;

		case 8:
		stepperCtx.regVal |= EIGHTH_STEP;
		break;

		case 16:
		stepperCtx.regVal |= SIXTEENTH_STEP;
		break;
		
		default:
		break;
	}
}

void stepper_setup_default()
{
	stepperConfig(10, 16, 2000 , 2000, 20000);
}

void stepper_run(uint32_t totalTravel, 
			    uint32_t numShots)
{
}

int stepperJog(struct ftdi_context* ftdic,
			   uint8_t direction,
			   uint32_t steps)
{
	uint32_t i;
	int32_t rc;
	uint8_t buf;
	if (direction)
	{
		stepperCtx.regVal |= DIR_BIT;
		printf("write 0x%x\n",(uint8_t) stepperCtx.regVal);
	}
	else
	{
		stepperCtx.regVal &= ~(DIR_BIT);
		printf("write 0x%x\n",(uint8_t) stepperCtx.regVal);
	}
	stepperCtx.regVal &= ~(_ENA_BIT);
	stepperCtx.regVal &= ~(STEP_BIT);
	rc = ftdi_write_data(ftdic, (uint8_t*)&stepperCtx.regVal, 1);
	for (i = 0; i < steps; i++)
	{
		stepperCtx.regVal |= (STEP_BIT);
	////	printf("write 0x%x\n",(uint8_t)stepperCtx.regVal);
		rc = ftdi_write_data(ftdic, (uint8_t*)&stepperCtx.regVal, 1);
		if (rc < 0)
		{
//			fprintf(stderr,"write failed for 0x%x, error %d (%s)\n",(uint8_t) stepperCtx.regVal,rc, ftdi_get_error_string(&ftdic));
		}
		usleep(1);
		stepperCtx.regVal &= ~(STEP_BIT);
		rc = ftdi_write_data(ftdic, (uint8_t*)&stepperCtx.regVal, 1);
		usleep(stepperCtx.delayPerStepFast);
//		usleep(1000000);
	}
}


void test(struct ftdi_context* ftdic)
{
	uint8_t buf;
	buf = 0x04;
	ftdi_write_data(ftdic, &buf, 1);
	sleep(100);
}

void shutter(struct ftdi_context* ftdic)
{
	int rc;
	stepperCtx.regVal |= (SHUTTER_BIT);
	rc = ftdi_write_data(ftdic, (uint8_t*)&stepperCtx.regVal, 1);
	usleep(100000);
	stepperCtx.regVal &= ~(SHUTTER_BIT);
	rc = ftdi_write_data(ftdic, (uint8_t*)&stepperCtx.regVal, 1);
}


int main(int argc, char **argv)
{
	struct ftdi_context ftdic;

    int f,i, count, dir;
    uint8_t buf;
	int stepsPerShot;
	int shotNum;


	if (init_ftdi(&ftdic) != 0)
	{
        return EXIT_FAILURE;
	}

	int mmPerShot = 15;
	int numShots = 10;
	stepperConfig(4,16, 2000, 1000, 200 );

	stepsPerShot = mmPerShot*stepperCtx.stepsPerFullStep/stepperCtx.mmPerFullStep;
	printf("stepsPerShot = %d\n", stepsPerShot);

	for (shotNum = 0; shotNum < numShots; shotNum++)
	{
		stepperJog(&ftdic, 0, stepsPerShot);
		usleep(stepperCtx.settleTimePerShot*1000);
		shutter(&ftdic);
	usleep(stepperCtx.settleTimePerShot*1000);
	}
	stepperJog(&ftdic, 1, stepsPerShot*numShots);

    ftdi_disable_bitbang(&ftdic);

    ftdi_usb_close(&ftdic);
    ftdi_deinit(&ftdic);
}

