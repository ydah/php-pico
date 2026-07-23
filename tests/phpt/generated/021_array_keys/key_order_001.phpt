--TEST--
associative key order vector 001
--FILE--
<?php
$items = ['left' => 11, 4 => 23, 'right' => 35];
unset($items[4]);
$items['tail'] = 36;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:11,35,36:1
