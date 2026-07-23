--TEST--
array helper vector 008
--FILE--
<?php
$values = [11, 23, 35];
$reverse = array_reverse($values); $slice = array_slice($values, 1, 2);
echo implode(',', $reverse), ':', implode(',', $slice), ':', array_sum($values), ':', array_product($values), ':', max($values), ':', min($values);
--EXPECT--
35,23,11:23,35:69:8855:35:11
