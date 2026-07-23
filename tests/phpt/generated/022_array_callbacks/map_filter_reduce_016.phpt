--TEST--
array callbacks vector 016
--FILE--
<?php
$values = [17, 18, 19, 20, 21]; $factor = 2;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
34,36,38,40,42:34,38,40:112
