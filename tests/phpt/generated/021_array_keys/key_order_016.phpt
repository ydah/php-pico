--TEST--
associative key order vector 016
--FILE--
<?php
$items = ['left' => 26, 4 => 68, 'right' => 110];
unset($items[4]);
$items['tail'] = 111;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:26,110,111:1
