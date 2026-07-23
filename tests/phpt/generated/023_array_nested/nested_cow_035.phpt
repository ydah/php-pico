--TEST--
nested array copy-on-write vector 035
--FILE--
<?php
$tree = ['node' => ['value' => 141, 'items' => [142]]];
$copy = $tree;
$copy['node']['value'] += 2;
$copy['node']['items'][] = 145;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
141:143:142:142,145
