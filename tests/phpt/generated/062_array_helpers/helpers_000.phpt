--TEST--
array helper vector 000
--FILE--
<?php
$values = [3, 7, 11];
$reverse = array_reverse($values); $slice = array_slice($values, 1, 2);
echo implode(',', $reverse), ':', implode(',', $slice), ':', array_sum($values), ':', array_product($values), ':', max($values), ':', min($values);
--EXPECT--
11,7,3:7,11:21:231:11:3
