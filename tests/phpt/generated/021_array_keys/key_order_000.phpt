--TEST--
associative key order vector 000
--FILE--
<?php
$items = ['left' => 10, 4 => 20, 'right' => 30];
unset($items[4]);
$items['tail'] = 31;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:10,30,31:1
