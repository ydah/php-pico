--TEST--
associative key order vector 033
--FILE--
<?php
$items = ['left' => 43, 4 => 119, 'right' => 195];
unset($items[4]);
$items['tail'] = 196;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:43,195,196:1
