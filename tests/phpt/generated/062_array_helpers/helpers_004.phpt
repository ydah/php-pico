--TEST--
array helper vector 004
--FILE--
<?php
$values = [7, 15, 23];
$reverse = array_reverse($values); $slice = array_slice($values, 1, 2);
echo implode(',', $reverse), ':', implode(',', $slice), ':', array_sum($values), ':', array_product($values), ':', max($values), ':', min($values);
--EXPECT--
23,15,7:15,23:45:2415:23:7
