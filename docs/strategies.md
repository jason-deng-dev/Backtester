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

build these 5 as swappable concerns, that we can mix and match to influence our strategy behavior

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

# Strategies to implement

|Pick |	What it stresses|
|----------|----------|
Buy & hold + random |	The benchmark and null every metric hangs off
Fast mean reversion (z-score)	|Turnover, cost sensitivity, per-exit accounting, huge trade counts
Slow trend (Donchian/MA cross)|	Long holds, MAE/MFE distributions, position carry
Vol-targeted sizing overlay	|Forces sizing to be a separate axis from the signal

# Details

## Random signal generator
- produces a random direction {-1,0,+1} that we can test against our strategy signals
- every time we test a real strategy, run it against random signals with the same trade frequency and hold times
- so we can see if our strategy's edge is real or just an artifact of the backtest

## Buy & hold
Period 0 use all of cash to buy maximum number of shares that we can afford
Period N sell everything

Signal
- produces +1 on first trading period
- produces -1 on last trading period

Sizing
- max amount I can buy when buying
- max amount I can sell when selling

Risk
- NaN

