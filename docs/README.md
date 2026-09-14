# Backtesting
- applying trading strategy to historical data to assess its profitability
- test trading ideas without risking real funds
- "what worked in the past may work in the future"

# workflow

As we iterate across historical stock data of companies over an event horizon, we test different strategies via "trader bots" that will run their stratgies with starting balance etc.

by having them execute based on their strategies we want to see after our data what is the resulting payoff

so we need to be able to tweak differnt parameters of the traders (the interface layer for client) to see how different strategies perform

# Fill timing
Next-bar timing
- signal from close of N -> order submitted -> fill on N+1
- compute after close, submit, fill at next open

# Assumptions
- constant risk-free annual rate
- prices are split + divided adjusted
- no slippage, no bid-ask spread, no market impact
- no commissions or fees
- no taxes
- idle cash earns 0
- 252 trading days for annualization
- integer share quantities
- margin rate = 1.0

# Open position at end of backtest handling
value remaining positions at the last bar's closing price, and include that unrealized P&L in the final equity
- the positions are not closed
- final portfolio value = cash + (shares * last close)

# Builds

## Build 1

### Data feed class
- reads from the stock historical data and feeds to our Backtest class

bool load(filePath)
- returns true if successful load, false if load failed
- copying the contents of the csv file into `std::vector<char>` buffer
- as well as setting up *cursor which points to the start of the data (after skipping 3 rows of header information in csv)

// precondition: the csv file we feed is made of complete rows, matching the signature we specify
bool next(&row)
- need to grab value from the current row, extracted via std::from_chars, and store in row
- return false if cursor is out of range or if extraction fails aborting the function call



### Strategy class
- class which contains parameters to tune the strategy we want to backtest

### State class
- which tracks what our strategy has generated in terms of at what point it bought, when it sold, the total profit/gain/loss and each point of the actions taken
- member data variables as described in `metrics.md`

### Analytics class
- which uses the information stored in State class to generate metrics we can use to grade the 
- Metrics are as described in `metrics.md`

### Backtest class
- the glue that calls all of the classes, to perform a backtest

# To download stock historical data
```bash
python -m venv .venv
.venv/bin/pip install yfinance
.venv/bin/python scripts/fetchdata.py 
```
