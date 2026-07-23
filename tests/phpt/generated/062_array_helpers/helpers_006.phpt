--TEST--
array helper vector 006
--FILE--
<?php
$values = [9, 19, 29];
$reverse = array_reverse($values); $slice = array_slice($values, 1, 2);
echo implode(',', $reverse), ':', implode(',', $slice), ':', array_sum($values), ':', array_product($values), ':', max($values), ':', min($values);
--EXPECT--
29,19,9:19,29:57:4959:29:9
