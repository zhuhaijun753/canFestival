#ifndef __CAN_H_INCLUDED
#define __CAN_H_INCLUDED

#include <stdint.h>
#include <inttypes.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <can_dcmd.h>

/* Event Flag Definitions */

/*sundh 对于控制模板程序上的缓存，依然让让一个IOM有512字节，
 * 但是如果从CNET上传上来的数据位置大于288，说明这个数据是不可靠的。因为PCS1800上一块IOM只有8个通道
 *
 */
#define PCS1800_IOM
//#define CHECKIOADDR	//检查收到的数据包中的IO地址的正确性

//#define	RELAXRECVFILTER//sundh 140421 放宽接收过滤的条件，在AO跳变中发现部分数据包不应该被过滤，
//141103 其实是写命令的应答，在DealFrame中把0xao应答改成了0xco,所以要屏蔽掉
#define CNETPACKETCRC	//sundh 140324 在CNET报文中增加CRC校验
#define TAKECARESOE		//sundh 140430 针对SOE模板要做特殊处理 :不做CRC校验，通道数为16个

//#define DUG_AOJUMP		//	ao输出跳变调试
#define AO06	(0x0E)
#define AO05	(0x0D)
#define AO04	(0x0C)

//#define DUG_IOTIMEJUMP	//模板IomTime跳变
//#define DUG_SOE		//sundh 140506 Soe模板的测试

//#define PRI_MBXIDCOUNT	//打印各个邮箱的接收数量
//#define OPTCANHOP	//优化CAN总线热插拔	会引起CNET状态显示异常
#define OPTCANTX	//把	TX邮箱的前半部分配置成一个队列

#define CANDEV_EVENT_QUEUED 	0x00000001
/* #define CANDEV_EVENT_ 		0x00000020 */
/* #define CANDEV_EVENT_ 		0x00000040 */
/* #define CANDEV_EVENT_ 		0x00000080 */
/* #define CANDEV_EVENT_ 		0x00000100 */

/* CAN Message Linked List Data Structure */
typedef struct _canmsg {
	CAN_MSG 			cmsg; 		/* CAN message */
	struct _canmsg 		*next; 		/* Pointer to next message */
	struct _canmsg 		*prev; 		/* Pointer to previous message */
} canmsg_t;

/**
 * @brief The CAN message structure
 * @ingroup can
 */
typedef struct {
  uint32_t cob_id;	/**< message's ID */
  uint8_t rtr;		/**< remote transmission request. (0 if not rtr message, 1 if rtr message) */
  uint8_t len;		/**< message's length (0 to 8) */
  uint8_t resv[2];
  uint8_t data[8]; /**< message's datas */
} appMessage_t;

/* Message List Data Structure */
typedef struct _canmsg_list {
	size_t 				len; 		/* Length of each msg in bytes */
	unsigned 			nmsg; 		/* Total number of messages in list */
	volatile unsigned 	msgcnt; 	/* Number of messages in use */
	canmsg_t 			*msgs; 		/* Array of messages defining the message list */
	canmsg_t 			*head; 		/* Head of list to add new messages */
	canmsg_t 			*tail; 		/* Tail of list to consume messages */
	intrspin_t 			lock; 		/* List locking mechanism */
} canmsg_list_t;

/* CAN Device Type - either transmit or receive */
typedef enum
{
	CANDEV_TYPE_RX,
	CANDEV_TYPE_TX
} CANDEV_TYPE;

/* Blocked Client Wait Structure */
typedef struct client_wait_entry
{
	int 						rcvid;	/* Blocked client's rcvid */
	struct client_wait_entry 	*next;	/* Pointer to next blocked client */
} CLIENTWAIT;

/* Blocked Client Wait Queue */
typedef struct client_waitq_entry
{
	CLIENTWAIT					*wait;	/* Head of client wait queue */
	int							cnt;	/* Number of clients waiting */
} CLIENTWAITQ;

/* CAN Init Device Structure */
typedef struct can_dev_init_entry
{
	CANDEV_TYPE 		devtype;		/* CAN device type */
	int 				can_unit; 		/* CAN unit number */
	int 				dev_unit; 		/* Device unit number */
	uint32_t 			msgnum; 		/* Number of messages */
	uint32_t 			datalen; 		/* Length of CAN message data */
} CANDEV_INIT;

/* Generic CAN Device Structure */
typedef struct can_dev_entry
{
	iofunc_attr_t 		attr;
	iofunc_mount_t 		mount;
	CANDEV_TYPE 		devtype;		/* CAN device type */
	int 				can_unit; 		/* CAN unit number */
	int 				dev_unit; 		/* Device unit number */
	canmsg_list_t		*msg_list;		/* Device message list */
	volatile uint32_t 	cflags; 		/* CAN device flags */
	struct sigevent 	event;			/* Device event */
	CLIENTWAITQ			waitq;			/* Client wait queue */
} CANDEV;

/* CAN Device Driver Implemented Functions */
typedef struct _can_drvr_funcs_t
{
	void 	(*transmit)(CANDEV *cdev);
	int 	(*devctl)(CANDEV *cdev, int dcmd, DCMD_DATA *data);
} can_drvr_funcs_t;

/* Resource Manager Info Structure */
typedef struct resmgr_info_entry
{
	dispatch_t 					*dpp;
	dispatch_context_t 			*ctp;
	int							coid;
	resmgr_attr_t 				resmgr_attr;
	resmgr_connect_funcs_t 		connect_funcs;
	resmgr_io_funcs_t 			io_funcs;
	can_drvr_funcs_t			*drvr_funcs;
} RESMGR_INFO;







/* Global resource manager info variable */
extern RESMGR_INFO rinfo;

/* Library functions available to driver */
void can_resmgr_init(can_drvr_funcs_t *drvr_funcs);
void can_resmgr_start(void);
void can_resmgr_create_device(CANDEV *cdev);
void can_resmgr_init_device(CANDEV *cdev, CANDEV_INIT *cinit);
struct sigevent *can_client_check(CANDEV *cdev);

/* Message list function prototypes */
canmsg_list_t *msg_list_create(unsigned nmsg, size_t len);
void msg_list_destroy(canmsg_list_t *ml);

/* Enqueue/Dequeue message from/to clients for use with io_write/io_read */
int msg_enqueue_client(canmsg_list_t *ml, resmgr_context_t *ctp, int offset);
int msg_dequeue_client(canmsg_list_t *ml, resmgr_context_t *ctp, int offset);

/* Enqueue/Dequeue the next message */
void msg_enqueue(canmsg_list_t *ml);
void msg_dequeue(canmsg_list_t *ml);

/* Get the next Enqueue/Dequeue message */
canmsg_t *msg_enqueue_next(canmsg_list_t *ml);
canmsg_t *msg_dequeue_next(canmsg_list_t *ml);

/* Mini-driver support functions */
canmsg_t *msg_enqueue_mdriver_next(canmsg_list_t *ml);
void msg_enqueue_mdriver(canmsg_list_t *ml);




/////////////////add by sundh 20121121
/******************************************************************************
**                       INTERNAL MACRO DEFINITIONS
******************************************************************************/
#define CAN_DATA_BYTES_MAX_SIZE           (8u)
#define TRANSMIT_MULTIPLE_MSG             (2u)
#define TRANSMIT_REMOTE_FRAME             (3u)
#define DCAN_NO_INT_PENDING               (0x00000000u)
#define TRANSMIT_SINGLE_MSG               (1u)
#define DCAN_ERROR_OCCURED                (0x8000u)
#define ENTER_KEY_PRESSED                 (0xD)
#define CAN_RX_MSG_ID                     (0u)
#define DCAN_BIT_RATE                     (1000000u)
#define DCAN_IN_CLK                       (24000000u)





////////////////////////////////////////////////////////////////////////////////////////////



#endif


__SRCVERSION( "$URL: http://svn/product/tags/internal/bsp/nto650/ti-am3517-evm/1.0.0/latest/lib/io-can/public/hw/can.h $ $Rev: 219996 $" )
