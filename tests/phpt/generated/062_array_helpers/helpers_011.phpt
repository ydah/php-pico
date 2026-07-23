--TEST--
array helper vector 011
--FILE--
<?php
$values = [14, 29, 44];
$reverse = array_reverse($values); $slice = array_slice($values, 1, 2);
echo implode(',', $reverse), ':', implode(',', $slice), ':', array_sum($values), ':', array_product($values), ':', max($values), ':', min($values);
--EXPECT--
44,29,14:29,44:87:17864:44:14
