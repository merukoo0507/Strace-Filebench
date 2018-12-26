## Strace-Filebench  <br /> 
  Use Strace to get the data of the workload in Filebench.  <br />
  使用"Strace"，去獲取Filebench的workload的一些資料...  <br />
  <br /> 
  <br /> 
  Strace: [Trace Tool] strace-2.5.20  <br /> 
  Filebench: filebench
  
# Strace  <br />
  1.修改strace.c裡面.tr的路徑  <br />
    尋找strace.c裡的”.tr” <br />
  <br />
  2.把路徑改成"指定路徑"  <br />
    由此,抓到的trace檔(.tr)會儲存在該路徑  <br />
  <br />
  -p pid  <br />
  跟踪指定的進程pid  <br />
  <br />
  -f 跟踪由fork呼叫所產生的子進程 <br />
  <br />
  3.執行 $ ./strace -p “pid” -f <br />
