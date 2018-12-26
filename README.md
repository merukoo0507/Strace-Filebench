# Strace-Filebench
  使用環境:
  1. Ubuntu 14.04 x86
  2. Strace: [Trace Tool] strace-2.5.20
  3. Filebench: filebench

  執行順序:
  1. 需要先執行filebench
  2. 再執行strace去取得workload的資料

## Filebench
(Reference)  
https://hyunyoung2.github.io/2016/12/23/Benchmark_Of_filebench/  
https://github.com/filebench/filebench/wiki/Writing-workload-models  

1.install  
$ git clone https://github.com/zfsonlinux/filebench.git  
$ cd filebench  

2.make  
$ ./configure    -- run shell  
$ make  
$ sudo make install -- with sudo  
  
disabling randomize_va_space with:  
echo 0 > /proc/sys/kernel/randomize_va_space    
或  
sudo sh -c "echo 0 > /proc/sys/kernel/randomize_va_space  
  
3.Open filebench  
$ filebench  
  
4.Run  
$ load varmail  
開始跑並加上限制時間  
$ run 10


## Strace
1.修改strace.c裡面.tr的路徑
  尋找strace.c裡的”.tr”

2.把路徑改成"指定路徑"
  由此,抓到的trace檔(.tr)會儲存在該路徑

3.執行 
$ ./strace -p “pid” -f

-p pid
跟踪指定的進程pid

-f 跟踪由fork呼叫所產生的子進程
