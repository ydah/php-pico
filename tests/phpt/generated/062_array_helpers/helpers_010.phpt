--TEST--
array helper vector 010
--FILE--
<?php
$values = [13, 27, 41];
$reverse = array_reverse($values); $slice = array_slice($values, 1, 2);
echo implode(',', $reverse), ':', implode(',', $slice), ':', array_sum($values), ':', array_product($values), ':', max($values), ':', min($values);
--EXPECT--
41,27,13:27,41:81:14391:41:13
