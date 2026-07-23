--TEST--
array helper vector 002
--FILE--
<?php
$values = [5, 11, 17];
$reverse = array_reverse($values); $slice = array_slice($values, 1, 2);
echo implode(',', $reverse), ':', implode(',', $slice), ':', array_sum($values), ':', array_product($values), ':', max($values), ':', min($values);
--EXPECT--
17,11,5:11,17:33:935:17:5
