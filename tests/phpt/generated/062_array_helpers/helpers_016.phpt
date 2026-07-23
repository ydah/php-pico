--TEST--
array helper vector 016
--FILE--
<?php
$values = [19, 39, 59];
$reverse = array_reverse($values); $slice = array_slice($values, 1, 2);
echo implode(',', $reverse), ':', implode(',', $slice), ':', array_sum($values), ':', array_product($values), ':', max($values), ':', min($values);
--EXPECT--
59,39,19:39,59:117:43719:59:19
