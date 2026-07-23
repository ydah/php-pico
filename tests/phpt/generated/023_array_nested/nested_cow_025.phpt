--TEST--
nested array copy-on-write vector 025
--FILE--
<?php
$tree = ['node' => ['value' => 101, 'items' => [102]]];
$copy = $tree;
$copy['node']['value'] += 6;
$copy['node']['items'][] = 109;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
101:107:102:102,109
