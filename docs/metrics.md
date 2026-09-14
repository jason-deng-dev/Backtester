# 2 ways to classify a trade

1. Per-Exit (per-sell) Acounting
```
each sell is treated as separate trade, matched against the entry price

Sell 1: 50 shares @ $60 vs. Entry @ $50 -> profit = +$500 -> Win

Sell 2: 50 shares @ $10 vs. Entry @ $50 -> loss = -$2000 -> Loss
Result: 1 win, 1 loss (50% win rate)
```
a "sell" here means one matched lot, not one fill — see per-lot FIFO matching below
2. Per-Position (Round-Trip Accounting)
```
entire position is treated as one trade that closes only when position returns to zero

Total proceeds: (50x$60) + (50x$10) = $3000+$500 = $3500
Total costs: 100x$50 = $5000
Net P&L: -$1,500 -> Loss
Result: 1 trade, 1 loss (0% win rate)
```
For the purposes of our backtesting, we track both Per-Position and Per-Exit statistics


# per-lot FIFO matching

Per-exit records are built by matching each exit fill against open lots, oldest first.

- every entry fill pushes a lot: `{entryTime, remainingQty, entryPrice}`
- every exit fill consumes from the front of the queue until the fill is exhausted or the queue empties
  - consume < front lot remainingQty -> front lot shrinks, stays at front
  - consume == front lot remainingQty -> lot popped
  - fill > total open qty -> queue empties, remainder opens a new lot on the opposite side at the fill price (this is a position flip)
- one ExitRecord per lot consumed, not per fill
  - sell 150 against lots of 100 / 30 / 20 -> 3 records, each with its own entryTime and entryPrice, all sharing the same time and exitPrice
  - qty per record = the portion of that lot consumed, not the fill qty
  - sum of qty across the records of one fill == the fill qty

# trade-close metrics
For each trade
- it opens when our shares != 0
- closes when shares == 0
- during the period when our trade is open (long)
  - we need to track all the shares that we bought (time, price, qty) FIFO
    - need to track total spent on buying
  - as we sell stocks track (time, price, qty) 
    - track total proceeds
- during periods when trade is open (short)
  - need to track how many shares we borrowed (need to pay back)
    - track proceeds from selling those shares (time, price, qty) FIFO
  - as we buy stocks track (time, price, qty) 
    - track total spent

```c
int netQty = 0; // signed: +long, -short
double cash = 0.0;

// stored during run
vector<{time,double equity, int netQty}> barSnapshots; // equity at each bar close = cash + netQty*price
vector<{time, int qty, double price}> executions; // every fill, qty signed

// for MFE and MAE, resets when netQty changes sign;
// updated every back, snapshot saved in executions at fill
double maxPrice = -inf;
double minPrice = inf; 

// per-exit — one record per matched lot (FIFO), not per fill
struct ExitRecord {time t, entryTime; int qty; double exitPrice, entryPrice;};
// qty is the consumed portion of that lot, magnitude only (no sign)
// pnl = direction * qty * (exitPrice - entryPrice)   // direction: +1 long, -1 short

// per-position
struct PositionRecord{time openTime, closeTime; int direction; double entryNotional, exitNotional;};
//pnl = exitNotional-entryNotional
```


- win rate 
  - for Per-Positon
  - for Per-Exit 
- avg win/loss 
  - Average win/loss = (total profit from winning/losing trades) / (number of winning/losing trades)
  - win/loss ratio = Average win / Average loss
- profit factor = gross profit / gross loss
- expectancy 
  - (win rate * avg win amount per winning trade) - (lose rate * avg loss amount per losing trade)
- total return 
- trade count

# path/curve metrics
- max drawdown
  - greatest movement from high point to low point in a portfolio
  - MDD = (Peak-Trough) / Peak
- Sharpe / Volatility
  - computed from daily returns then annaulized
  - need equity curve E_0, ... E_n
  - daily returns 
    - r_t = (E_t - E{t-1}) / E_{t-1}
  - mean
    - mean_r = Σ r_t / n
  - sample stddev
    - var = Σ (r_t - mean_r)² / (n - 1)
    - std = √var
  - risk-free 
    - rf_daily = rf_annual / 252
  - annualize
    - sharpe = (mean_r - rf_daily) / std * √252
- time-in-market
  - fraction of backtest you held a position (barSnapshots when netQty == 0)
- CAGR
  - constant annual rate that would take you from start equity to end equity, compounded
  - = (E_end / E_start) ^ (1/years)-1

# MAE/MFE
- MAE (Maximum adverse excursion)
  - worst unrealized loss at any point during the trade
- MFE (Maximum favorable excursion)
  - best unrealized gain at any point during trade

state holds for each bar where hold position:
Long: adverse_bar = low[t] - entry, favorable_bar = high[t] - entry
Short: adverse_bar = entry - high[t], favorable_bar = entry-low[t]

mae of position = min(adverse[t]) along period of position

mfe of position = max(favorable[t]) along period of position


