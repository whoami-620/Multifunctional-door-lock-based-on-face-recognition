#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <sched.h>

#define errExit(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

// ------------------------ raspi interface ----------------------------------

#define BLOCK_SIZE (4096)
 
struct bcm_peripheral {
    unsigned long addr_p;
    int mem_fd;
    void *map;
    volatile unsigned int *addr;
} gpio;

void map_peripheral(struct bcm_peripheral *p) {
	// get gpio address from system file (it differs between pi models)
	system("cat /proc/iomem | grep gpio | cut -f 1 -d \"-\" >GPIO_BASE");
	FILE *ff = fopen("GPIO_BASE","r");
	fscanf(ff,"%x",&p->addr_p);
	fclose(ff);
	printf("GPIO address: %08x\n",p->addr_p);
 	if ((p->mem_fd = open("/dev/mem",O_RDWR|O_SYNC))<0) {
		errExit("Failed to open /dev/mem, try checking permissions.\n");
	}
	p->map = mmap(			// mmap GPIO fn
		NULL,
		BLOCK_SIZE,
		PROT_READ|PROT_WRITE,
		MAP_SHARED,
		p->mem_fd,			// File descriptor to physical memory virtual file '/dev/mem'
		p->addr_p			// Address in physical map that we want this memory block to expose
	);
	if (p->map == MAP_FAILED) errExit("Failed to map peripherals\n");
	close(p->mem_fd);		// can close /dev/mem now
	p->addr = (volatile unsigned int *)p->map;
}
void unmap_peripheral(struct bcm_peripheral *p) {
	munmap(p->map, BLOCK_SIZE);
}

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g)  *(gpio.addr + ((g)/10)) &= ~(7<<(((g)%10)*3))	// sets 000
#define OUT_GPIO(g)  *(gpio.addr + ((g)/10)) |=  (1<<(((g)%10)*3))	// sets 001
 
#define GPIO_SET(g)   *(gpio.addr + 7) = (1<<g)	// sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR(g)   *(gpio.addr +10) = (1<<g)	// clears bits which are 1 ignores bits which are 0
 
#define GET_GPIO(g)  (*(gpio.addr+13)&(1<<g))	// 0 if LOW, (1<<g) if HIGH

static void set_op(int g) { INP_GPIO(g); OUT_GPIO(g); }	// set o/p mode

static void set_ip(int g) { INP_GPIO(g); }				// set i/p mode

// ------------------------------------------- dht22 stuff --------------------------------

#define DATA_PIN 17

static struct sched_param sched1, sched2;
static struct timespec t[128], lastread;
static int elap[128], bit[64], b[8], curtemp;
static int hprev, tprev, herrcnt, terrcnt;

static void set_max_pri() {
	memset(&sched1, 0, sizeof(sched1));
	// Use FIFO scheduler with highest priority for the lowest chance of the kernel context switching.
	sched1.sched_priority = sched_get_priority_max(SCHED_FIFO);
	sched_setscheduler(0, SCHED_FIFO, &sched1);
}

static void set_def_pri() {
	memset(&sched2, 0, sizeof(sched2));
	// Go back to default scheduler with default 0 priority.
	sched2.sched_priority = 0;
	sched_setscheduler(0, SCHED_OTHER, &sched2);
}

static int mvt2(int n) {
	for (int i=82;i>n;i--) t[i] = t[i-2];
	return(n+2);
}

static int durn(struct timespec t1, struct timespec t2) {
	return(((t2.tv_sec-t1.tv_sec)*1000000) + ((t2.tv_nsec-t1.tv_nsec)/1000));	// elapsed microsecs
}

void init_read_temp() {
	clock_gettime(CLOCK_REALTIME,&lastread);
	curtemp = 200;
	hprev = tprev = herrcnt = terrcnt= 0;
	map_peripheral(&gpio);
	set_op(DATA_PIN);							// set o/p mode
	GPIO_SET(DATA_PIN);							// high
}

int read_temp() {
	int state, err, n, i, sc, esc;
	struct timespec	st, cur;
	clock_gettime(CLOCK_REALTIME, &st);
	if (durn(lastread,st)<5000000) return(curtemp);		// 5 secs between readings
	lastread = st;
	memset(elap,0,sizeof(elap));
	memset(bit,0,sizeof(bit));
	memset(b,0,sizeof(b));
	memset(t,0,sizeof(t));
	GPIO_CLR(DATA_PIN);				// start pulse low for 3 msec
	do clock_gettime(CLOCK_REALTIME, &cur); while (durn(st,cur)<3000);
	set_max_pri();					// try to hog cpu
	GPIO_SET(DATA_PIN);				// end of start pulse
	set_ip(DATA_PIN);				// change to i/p mode
	clock_gettime(CLOCK_REALTIME, &st);
	i = err = esc = 0;
	while (!err && (state=GET_GPIO(DATA_PIN))==0) {	// wait for pin to go high
		clock_gettime(CLOCK_REALTIME, &cur);
		err = durn(st,cur)>10000;
	}
	// now expect 42 low edges + 41 high edges = 83 edges
	if (!err) do {
		clock_gettime(CLOCK_REALTIME, &st);
		esc=0;
		while (!esc && (n=GET_GPIO(DATA_PIN))==state) {
			clock_gettime(CLOCK_REALTIME, &cur);
			if (durn(st,cur)>300) esc++;			// must have an escape route
		}
		state=n;
		clock_gettime(CLOCK_REALTIME, t+i);
	} while (i++ < 83);
	set_def_pri();					// remove cpu hog
	set_op(DATA_PIN);				// set o/p mode
	GPIO_SET(DATA_PIN);				// high
	if (err) return(curtemp);
	// now calc the low and high durations
	for (i=0;i<82;i++) {
		if ((elap[i]=durn(t[i],t[i+1]))<0) elap[i]=0;
		// and adjust for low pulses which are too long
		if (i>=2 && (i&1)==0) {
			int low = (((i-2)%16)==0 && i>2) ? 68:58;	// low durn max
			n = elap[i];
			if (n>180) {
				// insert a '1' pair
				elap[i] = 52; elap[i+1] = 76; elap[i+2] = n-128;
				// also need to move the t[] cells down 2
				i = mvt2(i);
			} else if (n>130) {
				// insert a '0' pair
				elap[i] = 52; elap[i+1] = 26; elap[i+2] = n-78;
				// also need to move the t[] cells down 2
				i = mvt2(i);
			} else if (n>low) {
				// adjust current pair
				int nxh = durn(t[i+1],t[i+2]);		// high pulse length
				low -= 3;							// preferred low durn
				elap[i++] = low; elap[i] = nxh+(n-low);
			}
		}
	}
	// if low pulses are <50, borrow the diff from previous high
	// if low pulses are >60, donate the diff  to  previous high
	//     (figures are 63/73 for 1st bit of bytes after the first)
	for (i=2;i<82;i+=2) {
		int lim = (((i-2)%16)==0 && i>2) ? 68:55;
		n = elap[i];
		if (n<lim-5) {
			n = lim-n; elap[i] += n; elap[i-1] -= n;
		} else if (n>lim+5) {
			n -= lim; elap[i] -= n; elap[i-1] += n;
		}
	}
	// create bits from totals of low+high each side of 100 usec (ignoring start pulses)
	for (i=0;i<80;i+=2) bit[i/2] = (elap[i+2]+elap[i+3])<100 ? 0:1;
	// create bytes from bits
	for (i=0;i<40;i++) { b[i/8] <<= 1; b[i/8] |= bit[i]; }
	// calc s/c of bytes 0-3
	sc = (b[0]+b[1]+b[2]+b[3])&0xff;
	if (sc==b[4]) {						// reading valid is sumcheck correct
		int m=b[2]&0x80, hnow=(b[0]<<8)|b[1], tnow=((b[2]&0x7f)<<8)|b[3];
		int herr=0, terr=0;
		if (hprev) {								// if not first reading
			int hdiff = hnow - hprev;
			int tdiff = tnow - tprev;
			if (herr = (hdiff>45 || hdiff<-45)) herrcnt++;
			if (terr = (tdiff>15 || tdiff<-15)) terrcnt++;
		}
		if (!herr || herrcnt>1) { hprev = hnow; herrcnt=0; }
		if (!terr || terrcnt>1) { tprev = tnow; terrcnt=0; }
		if (!herr && !terr) curtemp = tnow;
	}
	return(curtemp);
}
