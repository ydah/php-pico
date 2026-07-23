--TEST--
array helper vector 009
--FILE--
<?php
$values = [12, 25, 38];
$reverse = array_reverse($values); $slice = array_slice($values, 1, 2);
echo implode(',', $reverse), ':', implode(',', $slice), ':', array_sum($values), ':', array_product($values), ':', max($values), ':', min($values);
--EXPECT--
38,25,12:25,38:75:11400:38:12
