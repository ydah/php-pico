--TEST--
array callbacks vector 034
--FILE--
<?php
$values = [35, 36, 37, 38, 39]; $factor = 4;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
140,144,148,152,156:140,148,152:440
