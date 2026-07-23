--TEST--
array helper vector 017
--FILE--
<?php
$values = [20, 41, 62];
$reverse = array_reverse($values); $slice = array_slice($values, 1, 2);
echo implode(',', $reverse), ':', implode(',', $slice), ':', array_sum($values), ':', array_product($values), ':', max($values), ':', min($values);
--EXPECT--
62,41,20:41,62:123:50840:62:20
