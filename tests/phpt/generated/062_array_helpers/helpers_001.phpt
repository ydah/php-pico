--TEST--
array helper vector 001
--FILE--
<?php
$values = [4, 9, 14];
$reverse = array_reverse($values); $slice = array_slice($values, 1, 2);
echo implode(',', $reverse), ':', implode(',', $slice), ':', array_sum($values), ':', array_product($values), ':', max($values), ':', min($values);
--EXPECT--
14,9,4:9,14:27:504:14:4
