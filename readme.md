# 2020 Parallel Programming HW1 Report

## Implementation

在實作過以一個 Float 當作 Element 單位的版本後，Communication Overhead 讓那個版本變的不可行，所以最後上網參考[這個網站](selkie-macalester.org/csinparallel/modules/MPIProgramming/build/html/oddEvenSort/oddEven.html)後，實作出了最終版本。

### Load Balancing

因為這次資料的特性，每個 Node 所處理的資料量越平均越好，所以採用這種 Ceiling 的形式，最後一個 Node 的數量會比較少，但不會有 Floor 方法所導致的最後一個 Node 單獨多很多資料。

```c++
n = atoi(argv[1]);
// 連這邊都會有問題，double 沒用的話，float 精度會在 testcases 32 出錯
partition = std::ceil(n / (double)size);
fIndex = partition * rank;
```

### Data Preprocessing

在真正進到 Odd-Even Sort，開始交換資料前，先處理了以下幾件事：

- 剛剛提到的 Load Balancing
- 透過 MPI-IO 讀取資料
- 分配記憶體給 3 個 Buffer，分別是資料存儲、交換用 Buffer、Copy 用 Buffer
- Sort 資料（使用 boost::sort::spreadsort::spreadsort） ＆ 其他相關變數計算

### Odd-Even Sort Phase

Odd-Event Sort 的實作方式，數值交換會分成 Even Phase & Odd Phase，Even Phase 時 Index 為偶數的元素會跟 Index 為 Odd 的元素（大於自己的 Index）比較，如果順序不對則會交換，Odd Phase 同樣的也是這樣處理，只是是以 Odd Index 為出發點。在 Parallel Version 中，我採用的是把每個 Process 當作一個元素單位來看待，再進入 Odd-Even Exchange Phase 前會先對每一個 Node 自己的資料排序，接下來每個 Phase 對的 Node 會與 Pair Node 互相交換自己擁有的資料，直到演算法結束為止。

中間溝通我還多了一道有關 Communication Protocol 優化的手續，後面優化的部分將會提到。

## Optimization

### 程碼撰寫優化

程式碼優化的部份我是參考 Optimized C++ by Kurt Guntheroth 這本書一些內容，有些東西真的有差，但有些優化可能是因為系統當下別狀況讓特定方法的成效並不是這麼明顯。對此次作業比較有幫助的是：

1. Loop 中執行邏輯盡量減少，固定數值不要重複計算
2. Memory Allocation 盡量減少，Function Call 的 Stack Allocation 或是 Dynamic Memory Allocation（尤其是 Dynamic 的部份），能提到非 Loop 的地方執行會有最棒的效能
3. 改用不同的 MPI Api，也許是因為 Library 底層實做時對 Send+Recv 這種 Communication Pattern 會特別優化，當使用兩個分開的 Send Recv 時，會比使用 Sendrecv 還要來的慢
4. 用 Ternary Operator 會比使用完整的 If Else 快一點，也許只是湊巧剛好，但估計是產出的 Assembly Code 會有不同

### 程式碼演算法優化

演算法優話的部份其實加速最多得是把每個 Process 當作一個 Odd-Even Sort 中的 Element，這麼做的目的其實最主要的是可以減少非常大量的溝通時間，假設我們的 Odd Even Sort 是真的使用一個數字當作 Element 的話，還是會需要經過 N 個 Iteration 的時間（沒有使用 Early Stop 的話），跟 Sequencial 的 Bubble Sort 的時間不相上下以外還要多一堆 Communication Time。

其他優化的部份都實做在其他剩餘的計算中，大致可以分為以下幾點：

1. 在 Merge 從 Pair Node 回傳的資料時，原先貪圖方便直接使用 Algorithm 中的 merge 函式再進行 Copy 的動作，但這樣其實就多浪費了一個 n/size 的執行時間，真正只需要把需要的資料 Copy 進 Tmp 中，最後在 Copy 進去就好了
2. 上述的 Merge 其實並不一定需要做，當從 Pair Node 那邊接收到的資料的最小值已經比 Data 中的最大值還要大的時候（Even Phase，Odd Phase 反過來），就可以不用更新了，而這個優化的實做方法可以是傳過來再檢查，或是開始真正大筆資料開始交換前先做一個 Preflight Check，先交換雙方的最大最小值，如果符合條件的話才開始真正的傳輸。原本想說多一個 Preflight Check 會浪費更多時間，但最後實驗做出來的確會讓小資料時慢了一點點，但大筆資料交換時效果就會開始出來。

## 實驗結果

### 設備 & Testing Dataset

做實驗的機器是使用的是課堂提供的，實驗 Data Set 使用的是 35.in 的資料，資料內容為 35.txt，總共資料數量為 536869888。

會使用這個資料集的原因是因為資料數量大小剛剛好，不會大到 Sequential 跑太久，但也不會小到看不出來 Parallel Version 的好處。

### 名詞定義

以下是幾個會在圖表中用到的名詞：

- Preprocessing Time: 程式開始執行前（進入 Loop 前）所花費的時間，包括各個 Buffer Dynamic Memory Allocation
- Partition Computation，排序自己擁有的資料，以及其他零零總總的初始計算。
- MPIIO Time: 各個 Node 讀取和寫入各個 Process 自己資料的時間。
- Communication Time: 各個程式之間的溝通資料所花費的時間。
- Single Node Computation Time: 處理與 Pair Node 傳過來的資料

時間計算的方法是採用 clock_gettime() with MONOTONIC 這個方法，單獨計算每個 Part 時會跟計算完整計算時間的程式碼分開寫，怕說 clock_gettime() 在計算時會花太多時間，導致完整計算整個程式所花費的時間時會有誤差。

### Strong Scalability

| | |
| ---- | ---- |
|![Single Node Strong Scalability](./img/Single%20Node%20Strong%20Scalability.png)*圖2, 單 Node 不同 Process 數量所畫出來的圖*  | ![Multiple Node Strong Scalability](./img/Multiple%20Node%20Strong%20Scalability.png)*圖3, 多個 Node，每個 Node 有 8 個 Process*|

圖 2 是以單 Node，從 1 個 Process 到 12 個 Process 當作 x 軸製程的 Strong Scalability 圖。會只使用單個 Node 的原因是我想減少 Network Interference 所造成的影響，雖然沒辦法避開進到 OS Network Stack 的問題但至少沒有經過網路線路。

從圖 2 中可以看到，我的程式碼 Scaling 的成果其實不太好，如果在配上圖 3 張單位是 Node 數量為 X 軸的圖來看效果其實真的滿不好的。

會有這樣的原因我覺的是因為 Odd Even Sort 在改成 Parallel 時，單 Node 在處理數據的時間並不會顯著的因為資料數量變少而時間大量降低，所以導致結果並不好看。

### 各個種類時間花費佔比 Time Profile

| | |
| --- | --- |
| ![](./img/Single%20Node%20Time%20Profile.png)*圖 4, 單 Node，12 個 Process 不同比較* | ![](./img/Mutiple%20Node%20Time%20Profile.png)*圖 5, 不同 Node 數，每個 Node 有 8 個 Process 比較* |

圖 4 一樣是單 Node 配上 12 Core 的處理，可以很明顯看到 Sequencial Version 花最多的時間在 Preprocessing Data 上（也就包含 Sort 所花的時間），而當今天有 12 個 Core 在執行 Sorting 時各個 Node 分配到的 Data 數量就會減少，佔用時間也就大量減少。

其他幾個時間所佔用的狀況看這張圖的變化不明顯（尤其是 Communication Time，單 Node 的問題），所以我又做了以下這個實驗：

圖 5 就可以看到，MPIIO 與 Preprocessing Time 有明顯下降的趨勢，可以很簡單的推論出這兩個時間跟每個 Node 需要處理的資料數量成正相關。每個 Node 花在 Merge 資料上的時間並沒有因為 Process 變多而明顯時間下降，可能是因為 Merge 演算法並不會因為資料改變有很大的變化。

比較特殊的是 Communication Time 並沒有大大的增加，這個原因是因為我們的演算法是平行交換，最多就交換 Size(Process 的數量) 次，所以即使每個 Node 增加上去，在 Node 數量增加以及每個 Node 資料數量減少的狀況下，一來一往時間也就變化不大了。

## Conclusion

這次 Assignment 花了比較多時間在優化 C++ 架構方面的處理，因為 Odd Even Sort 的演算法並沒有辦法改太多，所以這次時間進步的原因有很大的部份是來自於 C++ 程式碼撰寫的優話，演算法的部份頂多就 Merge 那邊做一些簡單的處理以及修改實做方式。

在優化的時候其實想過很多問題，從一開始簡單的把 loop 中沒必要的計算移出來以及 Function 中的變數宣告能提到面的都提到外面，到最後思考新的 Communication Protocol，一步一步慢慢把時間縮短然後 Scoreboard 名次漸漸往上的感覺真的很爽。

不過這次優化有個問題，我為了 Performance 的緣故在幾個地方跟原本之前寫 Code 的習慣有巨大的差距。第一個是 Memory 的問題，因為這次的 Tradeoff 是越快越好，所以 Memory 就開的非常的大；第二個是為了減少 Function 或 Loop 中的 Variable Declaration，我把一些 Variable 提到 Global 宣告，讓這份程式碼的 Coding Style 有點差。