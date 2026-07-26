# Experiment design

The research question is how `naive`, `double_buffer`, and `append_log`
preserve acknowledged diagnostic records when a deterministic NOR-flash
operation is interrupted.

Independent variables are strategy, cut failpoint, warning budget, record size,
flash geometry, operation duration, logical-write frequency, and storage size.
Dependent variables are recovery outcome, accepted corruption, lost records,
physical programmed bytes, erases, boot-scan time, and per-sector wear.

Controlled conditions include the workload, initial erased image, codec,
geometry, campaign seed, and oracle policy. Raw results use versioned JSONL;
CSV is a derived export.

Exploratory campaigns may vary ranges. Confirmatory campaigns use a checked-in
configuration fixed before execution. Monte Carlo uses explicit seeds and
reports count, mean, median, p95, maximum, and a binomial 95% interval for
categorical recovery rates. Exhaustive campaigns remain the primary evidence
for small operations.
