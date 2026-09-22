- [ ] Monte Carlo
- [ ] decouple maxDrawdown calculation from analytics, so it can be used to on Monte Carlo generated equity path
- [ ] add regime logic to Analytics
- [ ] Reshuffling of trade outcomes
  - [ ] sample with replacement, until create one full equity path
  - [ ] perform many times
- [ ] 






































- [x] Backtester
  - [x] MVP
    - [x] Data feed 
    - [x] State 
    - [x] Strategy
    - [x] Backtest
  - [x] Issues
    - [x] negative cash still buying
    - [x] fix fill timing contradiction
    - [x] instead of fixed share amount in naive_reversion_stratgy, use notional sizing
    - [x] correct reset for maxPrice/minPrice in Backtest::run
    - [x] correct condition to execute order in Backtest::run
    - [x] don't pass closePrice to entryCondition/exitCondition since they shouldn't be able to use it
    - [x] sign conversion error in position management in exitCondition
      - [x] resulting in ever increasing short position
  - [x] Analytics
    - [x] recordExecutions
      - [x] implement lot based FIFO for exitRecords
    - [x] recordInfo
      - [x] fill sharpeInfo::dailyReturns
      - [x] fill maxDrawDown
      - [x] fill tradingDays, daysInMarket
      - [x] fill startEquity, endEquity
    - [x] Implement MAE/MFE in backtest
      - [x] combine recordExecutions/recordEquityCurve into a single function recordInfo(executions, equityCurve)
      - [x] recordExecutions functionality goes into handleExecutions
      - [x] update mae and mfe in episodeState
      - [x] when position closes, add mae,mfe to positionRecords
    - [x] computeSharpe (fills info)
    - [x] computePositionRecords
    - [x] computeExitRecords
  - [x] Strategies
    - [x] Signal
      - [x] buy & hold
      - [x] z-score reversion
      - [x] moving average crossover
      - [x] random
    - [x] Sizer  
      - [x] Fixed fractional
      - [x] Vol target
        - [x] getStdDev use std::optional to return not ready
    - [x] Risk Manager
      - [x] notional cap
        - [x] flips reduction into a buy
        - [x] divide be zero reachable (sizerOutput)
      - [x] BracketRiskManger (stop-loss / take-profit)
  - [x] Move Signal/Sizer/RiskManager to their own .h files
  - [x] Replace old Strategy implementation with new
  - [x] State should manage and track it's own current position entry price
    - [x] state.getEntryPrice()
  - [x] Refactor so lookback history is responsbility of Derived classes to handle
    - [x] Signal
    - [x] Sizer
    - [x] RiskManager
  - [x] Tests
    - [x] DataFeed
    - [x] State 
    - [x] Strategy
      - [x] SignalTest
        - [x] RandomSignal
        - [x] BuyAndHold
        - [x] Z-score reversion
        - [x] MA crossover
      - [x] SizerTest
        - [x] Fixed fractional
        - [x] Vol target
      - [x] RiskManagerTest
        - [x] NotionalCap
        - [x] Bracket
    - [x] Backtest
    - [x] Analytics



