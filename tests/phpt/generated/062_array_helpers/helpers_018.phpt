--TEST--
array helper vector 018
--FILE--
<?php
$values = [21, 43, 65];
$reverse = array_reverse($values); $slice = array_slice($values, 1, 2);
echo implode(',', $reverse), ':', implode(',', $slice), ':', array_sum($values), ':', array_product($values), ':', max($values), ':', min($values);
--EXPECT--
65,43,21:43,65:129:58695:65:21
