--TEST--
associative key order vector 002
--FILE--
<?php
$items = ['left' => 12, 4 => 26, 'right' => 40];
unset($items[4]);
$items['tail'] = 41;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:12,40,41:1
