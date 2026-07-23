--TEST--
array helper vector 007
--FILE--
<?php
$values = [10, 21, 32];
$reverse = array_reverse($values); $slice = array_slice($values, 1, 2);
echo implode(',', $reverse), ':', implode(',', $slice), ':', array_sum($values), ':', array_product($values), ':', max($values), ':', min($values);
--EXPECT--
32,21,10:21,32:63:6720:32:10
