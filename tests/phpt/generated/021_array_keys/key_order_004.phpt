--TEST--
associative key order vector 004
--FILE--
<?php
$items = ['left' => 14, 4 => 32, 'right' => 50];
unset($items[4]);
$items['tail'] = 51;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:14,50,51:1
