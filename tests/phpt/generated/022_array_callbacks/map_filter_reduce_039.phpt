--TEST--
array callbacks vector 039
--FILE--
<?php
$values = [40, 41, 42, 43, 44]; $factor = 5;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
200,205,210,215,220:200,205,215,220:840
