#ifndef __AHT21_HANDLER_H__
#define __AHT21_HANDLER_H__

#define AHT21_H_OFFLINE_THRESHOLD   5


/* ① 状态类型 —— 你写一个 enum
   （driver 的 aht21_err_t 说的是"这一次通不通"；
     handler 要报的是"这段时间靠不靠得住" —— 两者不是一个东西）*/
typedef enum{
   AHT21_H_OK = 0,        //在线，这次读到了   → 数据新鲜，能用
   AHT21_H_STALE,     //在线，这次没读到   → 给上次的，标"可能过期"
   AHT21_H_OFFLINE,   //连续失败到阈值     → 旧数据也撤，明确"读不到"
   AHT21_H_PARAM     /* 调用方传了空指针 —— 这不是器件的问题，是【调用】的问题 */
} aht21_handler_status_t;

/* ② 对外函数 —— app 一次调用要同时拿到 温度 + 湿度 + 状态
   （出参几个？返回值用什么？分工会不会和 ① 打架？）*/
aht21_handler_status_t aht21_handler_read(float *temp, float *humi);

#endif

