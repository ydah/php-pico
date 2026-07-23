--TEST--
array helper vector 012
--FILE--
<?php
$values = [15, 31, 47];
$reverse = array_reverse($values); $slice = array_slice($values, 1, 2);
echo implode(',', $reverse), ':', implode(',', $slice), ':', array_sum($values), ':', array_product($values), ':', max($values), ':', min($values);
--EXPECT--
47,31,15:31,47:93:21855:47:15
