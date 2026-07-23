--TEST--
array helper vector 015
--FILE--
<?php
$values = [18, 37, 56];
$reverse = array_reverse($values); $slice = array_slice($values, 1, 2);
echo implode(',', $reverse), ':', implode(',', $slice), ':', array_sum($values), ':', array_product($values), ':', max($values), ':', min($values);
--EXPECT--
56,37,18:37,56:111:37296:56:18
