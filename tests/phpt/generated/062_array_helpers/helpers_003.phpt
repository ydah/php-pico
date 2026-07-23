--TEST--
array helper vector 003
--FILE--
<?php
$values = [6, 13, 20];
$reverse = array_reverse($values); $slice = array_slice($values, 1, 2);
echo implode(',', $reverse), ':', implode(',', $slice), ':', array_sum($values), ':', array_product($values), ':', max($values), ':', min($values);
--EXPECT--
20,13,6:13,20:39:1560:20:6
