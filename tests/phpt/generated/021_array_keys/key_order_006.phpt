--TEST--
associative key order vector 006
--FILE--
<?php
$items = ['left' => 16, 4 => 38, 'right' => 60];
unset($items[4]);
$items['tail'] = 61;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:16,60,61:1
