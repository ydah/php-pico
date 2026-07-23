--TEST--
array helper vector 005
--FILE--
<?php
$values = [8, 17, 26];
$reverse = array_reverse($values); $slice = array_slice($values, 1, 2);
echo implode(',', $reverse), ':', implode(',', $slice), ':', array_sum($values), ':', array_product($values), ':', max($values), ':', min($values);
--EXPECT--
26,17,8:17,26:51:3536:26:8
