Rainbow.extend('icarus', [
  {
    'matches': {
      1: {
        'name': 'keyword.operator',
        'pattern': /\=/g
      },
      2: {
        'name': 'string',
        'matches': {
          'name': 'constant.character.escape',
          'pattern': /\\('|"){1}/g
        }
      }
    },
    'pattern': /(\(|\s|\[|\=|:)((")([^\\\1]|\\.)*?(\3))/gm
  },
  {
    'name': 'comment',
    'pattern': /\/\*[\s\S]*?\*\/|(\/\/|\#)[\s\S]*?$/gm
  },
  {
    'name': 'constant.numeric',
    'pattern': /\b\d+(\.\d+)?\b/gi
  },
  {
    'matches': {
      1: 'keyword'
    },
    'pattern' : /\b(b(ool|reak)|c(ase|har|ontinue)|else|if|r(eturn|eal)|string|this|u?int|void|while)(?=\(|\b)/g
  },
  {
    'name'    : 'boolean',
    'pattern' : /true|false/g
  },
  {
    'name'    : 'operator',
    'pattern' : /([+/\-*%|!:=]|&(gt|lt|amp);)=?|@|\.\.|in/g
  },
  {
    'name'    : 'rocket',
    'pattern' : /=&gt;/g
  },
  {
    'matches': {
      1: 'function.call'
    },
    'pattern': /(\w+?)(?=\()/g
  },
    ]);
