--TEST--
array helper vector 013
--FILE--
<?php
$values = [16, 33, 50];
$reverse = array_reverse($values); $slice = array_slice($values, 1, 2);
echo implode(',', $reverse), ':', implode(',', $slice), ':', array_sum($values), ':', array_product($values), ':', max($values), ':', min($values);
--EXPECT--
50,33,16:33,50:99:26400:50:16
