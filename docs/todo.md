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
- [ ] Analytics
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






  - [ ] Add concurrency/parallelism
    - [ ] to Analytic operations
    - [ ] to allow backtest on multiple historical data, on multiple strategies at once
  - [ ] 
