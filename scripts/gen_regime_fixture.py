#!/usr/bin/env python3
"""Generate tests/fixtures/regimeTest_bars.csv + golden regime labels.

Synthetic 70-bar series with known regime structure:

    d1 -d14  calm      (all WARMUP: needs returnLookback + volLookback bars)
    d15-d24  volatile  (d15 is the FIRST labeled bar: pins "first bar speaks
                        for itself" -> must be labeled VOLATILE immediately)
    d25-d44  calm      (V->C flip, long CalmToCalm run)
    d45-d59  volatile  (C->V flip)
    d60-d70  calm      (V->C flip, ends CALM)

The oracle below reimplements the C++ semantics exactly (RollingWindow
running sums, sample stddev, ceil-percentile with min-sample gate,
feed-before-label, hysteresis) so the printed label string is the ground
truth the gtest in tests/montecarlo_test.cc asserts against.

Regenerate after changing classifyRegime semantics:
    python3 scripts/gen_regime_fixture.py
"""

import math
import random
from collections import deque
from pathlib import Path

# must match the arguments passed to classifyRegime in the test
VOL_PERCENTILE = 0.75
CALM_PERCENTILE = 0.6
RETURN_LOOKBACK = 5
VOL_LOOKBACK = 10

SEGMENTS = [(14, "calm"), (10, "vol"), (20, "calm"), (15, "vol"), (11, "calm")]
CALM_AMP = (0.002, 0.008)
VOL_AMP = (0.02, 0.06)
SEED = 3

OUT = Path(__file__).resolve().parent.parent / "tests" / "fixtures" / "regimeTest_bars.csv"


class RollingWindow:
    """Mirrors include/rollingwindow.h: running sum/sumSq, deque eviction."""

    def __init__(self, max_size):
        self.max_size = max_size
        self.dq = deque()
        self.sum = 0.0
        self.sumsq = 0.0

    def add(self, v):
        self.dq.append(v)
        self.sum += v
        self.sumsq += v * v
        if len(self.dq) > self.max_size:
            front = self.dq.popleft()
            self.sum -= front
            self.sumsq -= front * front

    def stddev(self):
        n = len(self.dq)
        if n < self.max_size:
            return None
        return math.sqrt((self.sumsq - self.sum * self.sum / n) / (n - 1))

    def percentile(self, p):
        if len(self.dq) < self.max_size:
            return None
        return percentile(sorted(self.dq), p)


def percentile(v, p):
    """Mirrors percentile() in include/analytics.h."""
    if not v or p < 0 or p > 1:
        return None
    if p >= 1.0:
        return v[-1]
    min_needed = math.ceil(1.0 / (1.0 - p) - 1e-9) * 2
    n = len(v)
    if n < min_needed:
        return None
    k = min(max(math.ceil(p * n), 1), n)
    return v[k - 1]


def gen_closes():
    rng = random.Random(SEED)
    closes = [100.0]  # bar d1 belongs to the first segment
    for length, kind in SEGMENTS:
        lo, hi = CALM_AMP if kind == "calm" else VOL_AMP
        n = length - 1 if len(closes) == 1 else length
        for _ in range(n):
            r = rng.uniform(lo, hi) * rng.choice([-1, 1])
            closes.append(closes[-1] * (1 + r))
    # round to 6dp: from_chars and float() parse the same decimal back to
    # the same double, so oracle and C++ see identical values
    return [float(f"{c:.6f}") for c in closes]


def classify(equities):
    """Mirrors MonteCarlo::classifyRegime. equities[0] is the sentinel (0.0)."""
    ret_win = RollingWindow(RETURN_LOOKBACK)
    vol_win = RollingWindow(VOL_LOOKBACK)
    labels = []
    state = "C"
    prev_equity = 0.0
    index = 0
    for e in equities:
        index += 1
        # returns only between real bars; barSnapshots[0] is the sentinel
        if index > 2 and prev_equity != 0:
            ret_win.add(e / prev_equity - 1.0)
            v = ret_win.stddev()
            if v is not None:
                vol_win.add(v)
        prev_equity = e
        cur_vol = ret_win.stddev()
        high = vol_win.percentile(VOL_PERCENTILE)
        low = vol_win.percentile(CALM_PERCENTILE)
        if cur_vol is None or high is None or low is None:
            labels.append("W")
        else:
            if state == "C" and cur_vol > high:
                state = "V"
            elif state == "V" and cur_vol < low:
                state = "C"
            labels.append(state)
    return labels


def main():
    closes = gen_closes()
    with OUT.open("w") as f:
        f.write("Price,Close,High,Low,Open,Volume\n")
        f.write("Ticker,SYNTH,SYNTH,SYNTH,SYNTH,SYNTH\n")
        f.write("Date,,,,\n")
        prev = closes[0]
        for i, c in enumerate(closes, start=1):
            hi = max(prev, c) * 1.001
            lo = min(prev, c) * 0.999
            f.write(f"d{i},{c:.6f},{hi:.6f},{lo:.6f},{prev:.6f},100\n")
            prev = c

    # oracle runs on the exact CSV values; equities[0] = sentinel like State{0,1}
    labels = classify([0.0] + closes)
    label_str = "".join(labels[1:])  # drop sentinel, align with d1..dN

    print(f"wrote {OUT} ({len(closes)} bars)")
    print(f"expected labels ({len(label_str)}):")
    print(label_str)
    # segment summary so a human can sanity-check the pattern
    runs = []
    for i, lab in enumerate(label_str, start=1):
        if not runs or runs[-1][0] != lab:
            runs.append([lab, i, i])
        else:
            runs[-1][2] = i
    print("runs:", ", ".join(f"{lab}:d{a}-d{b}" for lab, a, b in runs))


if __name__ == "__main__":
    main()
