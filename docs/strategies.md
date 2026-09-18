# what makes up a strategy
- when to enter
  - every strategy needs a reason to believe it has positive expectancy
  - the entry encodes why you think you will make money
- when to exit
  - stop loss "I'm wrong, get me out"
  - Take profit "I'm right, lock it in"
  - or signal-based exit
- how much to risk

5 axis that make a strategy

1. Signal : lookback window(s), threshhold/z-score entry level, smoothing, confirmation bars
2. Sizing : fixed notional, fixed fractional, vol targeting (target % vol -> scale by 1/σ), Kelly fraction, equal-weight across N
3. Risk : stop-loss, take-profit, time stop, max holding period, max positions, drawdown kill-switch
4. Execution : entry timing (next open vs close), rebalance frequency, participation limit, slippage/cost model
5. Universe : which symbols, liquidity/price filters, rebalance universe refresh

# Structure

## implementation 1
Base class Strategy is an Abstract class
- getMove(State, open price, close price) 
  - called by Backtest class which specifies whether to buy or sell
  - open price is used to decide strategy
  - close price added to priceHistory (to be used for later moves)
- has entryConditions(), exitConditions()
  - called by getMove to determine action to perform
- has price history, keeping track of past N number of prices
  - Derived classes pass up how much price history they need to use
- 

specifc strategies are created by creating a derived class of Strategy, and implementing the getMove(), exitCondition() and entryCondition() functions

## implementation 2
Strategy class is called by backtest to get orders to execute

Strategy class itself is made up of 3 componenents that contribute to it's behavior an decisions
- Signal: determines when to enter/exit a position, long or short
- Sizing: determines the size of your entrance
- Risk: constraints and overrides that can veto, scale or force-exit a position 

Strategy flow:
|Layer |	Question to answer |Output|
|----------|----------|----------|
Signal| long/short/flat? | Direction {-1,0,+1}
Sizing | how much capital in each stock, size of our buy/sell | Target weights {5% AAPL, 3% NVDA}, magnitude of order
Risk | Am I alllowed to hold this, and at what size | Adjust weights / force exits / halts

Backtest holds `std::vector<Bar> history` which is passed to Strategy, who passes it to Signal/Sizer/RiskManager
`signalOutput = signal_.generate(state, history);`
`sizerOutput = sizer_.generate(signalOutput, state, history)`
`finalMove = riskManager_.generate(sizerOutput, state, history)`

# Strategies to implement

## Signal:
|signal |	What it stresses|
|----------|----------|
buy & hold | baseline
random | determinism — seeded, reproducible; plus the null-matching harness
z-score reversion | rolling-window history; high turnover; per-exit accounting
MA cross over | nested lookbacks (fast/slow), cross/edge detection


## Sizer:
| behavior |	What it stresses|
|----------|----------|
Fixed fractional | shares = (f x equity) / price 
Vol target: shares | (targetVol / σ) × equity / price

## Risk Manager:
| behavior | What it stresses|
|----------|-----------------|
notional/exposure cap|the risk↔sizer interface — cheapest proof the axis does anything
stop-loss (take-profit) | intrabar fill modelling

# Implementation Details

## Matched Null (Random signal)
- every time we test a real strategy, run it against random signals with the same trade frequency and hold times
- so we can see if our strategy's edge is real or just an artifact of the backtest

if flat, with prob p_enter, emit an entry impulse +1 or -1 (weighted by real strategy's long/short split), otherwise 0
if in position, with prob p_exit, emit exit impulse, otherwise 0

using std:mt19937 seeded via constructor so runs are reprodicble.

std::direcrete_distribution for weighted direction draw (long or short)

to get p_enter/p_exit:
run real strategy once, count transititions on sign of netQty
p_enter = (number of flat bars => entry) / (number of flat bars)
p_exit = (number of in-position bars => exit) / (number of in posiiton bars)

## Z-score mean-reversion
when series moves usually far from it's recent average, it will eventually move back toward that average

nMean = rolling mean over N bars
pCurr = current price
nStd = rolling standard deviation over last N bars
z = how many standard deviation pCurr is from its recent mean

z = (pCurr-nMean)/nStd

z = 0 : at the recent average
z = +2 : 2 standard deviations above average
z = -2 : 2 standard deviations above average

high z -> overbought -> expect price to fall -> short
low z -> oversold -> expect price to sell -> long
near z = 0 -> no edge -> flat or exit

Long:
entry: z <= -entryZ
exit:  z >= exitZ

Short:
entry: z >= entryZ
exit:  z <= -exitZ 


parameters:
N lookback
entryZ = 2.0
exitZ = 0.0

## Moving Average crossover
trend following signal
- compares a fast moving average and slow moving average
- when fast avg crosses above slow ave, trend is bullish
- when fast avg cross below slow avg, trend is bearish

parameters:
fastN: short lookback (ex: 20)
slowN: long lookback  (ex: 50)

fastAvg = moving avg(past fastN history)
slowAvg = moving avg(past slowN history)

Types of moving averages:

Simple moving average (SMA)
SMA_t = (1/N)∑(i=0 to N-1) P_{t-i}

Exponential Moving Average (EMA)
EMA_t = x*P_t + (1-x)EMA_{t-1}
x = 2/(N+1)

EMA reacts faster to recent prices than SMA, many crossover use EMA for fast line, and SMA or EMA for slow line

if fastAvg > slowAvg : uptrend -> long
if fastAvg < slowAvg : downtrend -> short or flat

enter long when fast cross above slow
enter short when fast cross below slow

## Bracket Risk Manager
If doesn't have N periods yet, just pass Sizer output unchanged

Using ATR (Average True Range)
- denominates the stop in units of the instrument's own volatiliy

TR = max(
    High - Low,
    |High - Previous Close|,
    |Low  - Previous Close|
)

ATR = smoothed average over True Range over N periods

ATR_N = mean(TR_1, ... TR_N)
ATR_{N+1} = (ATR_N × (N-1) + TR_{N+1}) / N

stop distance = stop Loss multiplier * ATR 
take distance = take profit multiplier * ATR

Long:
stop price = entry price - stop distance 
take price = entry price + take distance

Short:
stop price = entry price + stop distance
take price = entry price - take distance 

Behavior:
- stops are evaulated on close price
- Stop hits before Take (assume worst cast, since we can't know from data which hit first)
- takes precedence over




# Handling insufficient history data
if we don't have enough bar data stored inside `std::vector<Bar>` history passed to Signal/Sizer/RiskManager, to satisify minLookback (set at construction of Signal/Sizer/RiskManager)

Signals, Sizer and RiskManager will return 0, meaning the decision of the strategy on that bar is to do nothing

precondition is that in components that require lookback to perform there generate() function we need to have a check that
`history.size() >= minLookback` if not, return 0

